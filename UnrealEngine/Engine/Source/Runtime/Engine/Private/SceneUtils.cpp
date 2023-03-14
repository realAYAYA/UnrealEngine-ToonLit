// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneUtils.h"
#include "RHI.h"
#include "Engine/Scene.h"
#include "RenderUtils.h"

DEFINE_LOG_CATEGORY(LogSceneUtils);

bool IsMobileHDR()
{
	static auto* MobileHDRCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR"));
	return MobileHDRCvar->GetValueOnAnyThread() == 1;
}

bool IsMobilePropagateAlphaEnabled(EShaderPlatform Platform)
{
	return IsMobilePlatform(Platform) && (FPlatformMisc::GetMobilePropagateAlphaSetting() > 0);
}

ENGINE_API EMobileHDRMode GetMobileHDRMode()
{
	EMobileHDRMode HDRMode = EMobileHDRMode::EnabledFloat16;

	if (!IsMobileHDR())
	{
		HDRMode = EMobileHDRMode::Disabled;
	}
	
	return HDRMode;
}

ENGINE_API bool IsMobileColorsRGB()
{
	static auto* MobileUseHWsRGBEncodingCVAR = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.UseHWsRGBEncoding"));
	const bool bMobileUseHWsRGBEncoding = (MobileUseHWsRGBEncodingCVAR && MobileUseHWsRGBEncodingCVAR->GetValueOnAnyThread() == 1);

	return !IsMobileHDR() && bMobileUseHWsRGBEncoding;
}

ENGINE_API EAntiAliasingMethod GetDefaultAntiAliasingMethod(const FStaticFeatureLevel InFeatureLevel)
{
	EAntiAliasingMethod AntiAliasingMethod = EAntiAliasingMethod::AAM_None;

	if (InFeatureLevel == ERHIFeatureLevel::ES3_1)
	{
		static auto* MobileAntiAliasingCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.AntiAliasing"));
		AntiAliasingMethod = EAntiAliasingMethod(FMath::Clamp<int32>(MobileAntiAliasingCvar->GetValueOnAnyThread(), 0, AAM_MAX));

		static auto* MobileHDRCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR"));
		// Disable antialiasing in GammaLDR mode to avoid jittering.
		if (MobileHDRCvar->GetValueOnAnyThread() == 0 && AntiAliasingMethod != EAntiAliasingMethod::AAM_MSAA)
		{
			AntiAliasingMethod = EAntiAliasingMethod::AAM_None;
		}
	}
	else
	{
		static auto* DefaultAntiAliasingCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AntiAliasingMethod"));
		AntiAliasingMethod = EAntiAliasingMethod(FMath::Clamp<int32>(DefaultAntiAliasingCvar->GetValueOnAnyThread(), 0, AAM_MAX));
	}

	if (AntiAliasingMethod == AAM_MAX)
	{
		ensureMsgf(false, TEXT("Unknown anti-aliasing method."));
		AntiAliasingMethod = AAM_None;
	}

	static auto* MSAACountCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MSAACount"));

	EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(InFeatureLevel);

	if (AntiAliasingMethod == EAntiAliasingMethod::AAM_None)
	{
		// NOP
	}
	else if (AntiAliasingMethod == EAntiAliasingMethod::AAM_FXAA)
	{
		// NOP
	}
	else if (AntiAliasingMethod == EAntiAliasingMethod::AAM_TemporalAA)
	{
		// NOP
	}
	else if (AntiAliasingMethod == EAntiAliasingMethod::AAM_MSAA)
	{
		if (MSAACountCVar->GetValueOnAnyThread() <= 0)
		{
			AntiAliasingMethod = EAntiAliasingMethod::AAM_None;
		}
		else if ((InFeatureLevel >= ERHIFeatureLevel::SM5 && !IsForwardShadingEnabled(ShaderPlatform))
			|| (IsMobilePlatform(ShaderPlatform) && IsMobileDeferredShadingEnabled(ShaderPlatform)))
		{
			// Disable antialiasing for deferred renderer
			AntiAliasingMethod = EAntiAliasingMethod::AAM_None;
		}
	}
	else if (AntiAliasingMethod == EAntiAliasingMethod::AAM_TSR)
	{
		if (!SupportsTSR(ShaderPlatform))
		{
			// Fallback to UE4's TAA if TSR isn't supported on that platform
			AntiAliasingMethod = AAM_TemporalAA;
		}
	}
	else
	{
		unimplemented();
	}

	return AntiAliasingMethod;
}

ENGINE_API uint32 GetDefaultMSAACount(const FStaticFeatureLevel InFeatureLevel, uint32 PlatformMaxSampleCount/* = 8*/)
{
	uint32 NumSamples = 1;

	EAntiAliasingMethod AntiAliasingMethod = GetDefaultAntiAliasingMethod(InFeatureLevel);

	if (AntiAliasingMethod == EAntiAliasingMethod::AAM_MSAA)
	{
		static auto* MSAACountCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MSAACount"));

		EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(InFeatureLevel);

		if (IsForwardShadingEnabled(ShaderPlatform) || (IsMobilePlatform(ShaderPlatform) && !IsMobileDeferredShadingEnabled(ShaderPlatform)))
		{
			NumSamples = FMath::Max(1, MSAACountCVar->GetValueOnAnyThread());


			NumSamples = FMath::Min(NumSamples, PlatformMaxSampleCount);

			if (NumSamples != 1 && NumSamples != 2 && NumSamples != 4 && NumSamples != 8)
			{
				UE_LOG(LogSceneUtils, Warning, TEXT("Requested %d samples for MSAA, but this is not supported; falling back to 1 sample"), NumSamples);
				NumSamples = 1;
			}
		}

		if (NumSamples > 1)
		{
			bool bRendererSupportMSAA = false;

			FString FailedReason = TEXT("");

			bool bRHISupportsMSAA = RHISupportsMSAA(ShaderPlatform);

			if (InFeatureLevel == ERHIFeatureLevel::ES3_1)
			{
				bool bMobilePixelProjectedReflection = IsUsingMobilePixelProjectedReflection(ShaderPlatform);
				
				static const auto MobileEarlyZPassCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.EarlyZPass"));
				const bool bMobileAmbientOcclusion = IsMobileAmbientOcclusionEnabled(ShaderPlatform);
				const bool bMobileUsesShadowMaskTexture = MobileUsesShadowMaskTexture(ShaderPlatform);
				bool bIsFullDepthPrepassEnabled = bMobileAmbientOcclusion || bMobileUsesShadowMaskTexture || MobileEarlyZPassCVar->GetValueOnAnyThread() > 0;

				bRendererSupportMSAA = bRHISupportsMSAA && !bMobilePixelProjectedReflection && !bIsFullDepthPrepassEnabled;

				if (!bRendererSupportMSAA)
				{
					FailedReason = FString::Printf(TEXT("RHISupportsMSAA %d, MobilePixelProjectedReflection %d, MobileAmbientOcclusion %d, MobileUsesShadowMaskTexture %d, MobileEarlyZPass %d"), bRHISupportsMSAA ? 1 : 0, bMobilePixelProjectedReflection ? 1 : 0, bMobileAmbientOcclusion ? 1 : 0, bMobileUsesShadowMaskTexture? 1 : 0, MobileEarlyZPassCVar->GetValueOnAnyThread());
				}
			}
			else
			{
				bRendererSupportMSAA = bRHISupportsMSAA;

				if (!bRendererSupportMSAA)
				{
					FailedReason = FString::Printf(TEXT("RHISupportsMSAA %d"), bRHISupportsMSAA ? 1 : 0);
				}
			}

			if (!bRendererSupportMSAA)
			{
				NumSamples = 1;

				static bool bWarned = false;

				if (!bWarned)
				{
					bWarned = true;
					UE_LOG(LogSceneUtils, Log, TEXT("Requested %d samples for MSAA, but the platform doesn't support MSAA, failed reason : %s"), NumSamples, *FailedReason);
				}
			}
		}
	}

	return NumSamples;
}
