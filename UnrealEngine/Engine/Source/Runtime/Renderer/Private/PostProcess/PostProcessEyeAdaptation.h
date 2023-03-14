// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScreenPass.h"

// LuminanceMax is the amount of light that will cause the sensor to saturate at EV100.
//  See also https://en.wikipedia.org/wiki/Film_speed and https://en.wikipedia.org/wiki/Exposure_value for more info.
FORCEINLINE float EV100ToLuminance(float LuminanceMax, float EV100)
{
	return LuminanceMax * FMath::Pow(2.0f, EV100);
}

FORCEINLINE float EV100ToLog2(float LuminanceMax, float EV100)
{
	return EV100 + FMath::Log2(LuminanceMax);
}

FORCEINLINE float LuminanceToEV100(float LuminanceMax, float Luminance)
{
	return FMath::Log2(Luminance / LuminanceMax);
}

FORCEINLINE float Log2ToEV100(float LuminanceMax, float Log2)
{
	return Log2 - FMath::Log2(LuminanceMax);
}

// For converting the auto exposure to new values from 4.24 to 4.25
float CalculateEyeAdaptationParameterExposureConversion(const FPostProcessSettings& Settings, const bool bExtendedLuminanceRange);

// figure out the LuminanceMax (i.e. how much light in cd/m2 will saturate the camera sensor) from the CVar lens attenuation
float LuminanceMaxFromLensAttenuation();

// Returns whether the auto exposure method is supported by the feature level.
bool IsAutoExposureMethodSupported(ERHIFeatureLevel::Type FeatureLevel, EAutoExposureMethod AutoExposureMethodId);

// Returns true if the view is in a debug mode that disables all exposure
bool IsAutoExposureDebugMode(const FViewInfo& View);

// Returns the fixed exposure value
float CalculateFixedAutoExposure(const FViewInfo& View);

// Returns the manual exposure value
float CalculateManualAutoExposure(const FViewInfo& View, bool bForceDisablePhysicalCamera);

// Returns the exposure compensation from the View, 
float GetAutoExposureCompensationFromSettings(const FViewInfo& View);

bool IsExtendLuminanceRangeEnabled();

// Returns the auto exposure method enabled by the view (including CVar override).
EAutoExposureMethod GetAutoExposureMethod(const FViewInfo& View);

BEGIN_SHADER_PARAMETER_STRUCT(FEyeAdaptationParameters, )
	SHADER_PARAMETER(float, ExposureLowPercent)
	SHADER_PARAMETER(float, ExposureHighPercent)
	SHADER_PARAMETER(float, MinAverageLuminance)
	SHADER_PARAMETER(float, MaxAverageLuminance)
	SHADER_PARAMETER(float, ExposureCompensationSettings)
	SHADER_PARAMETER(float, ExposureCompensationCurve)
	SHADER_PARAMETER(float, DeltaWorldTime)
	SHADER_PARAMETER(float, ExposureSpeedUp)
	SHADER_PARAMETER(float, ExposureSpeedDown)
	SHADER_PARAMETER(float, HistogramScale)
	SHADER_PARAMETER(float, HistogramBias)
	SHADER_PARAMETER(float, LuminanceMin)
	SHADER_PARAMETER(float, LocalExposureHighlightContrastScale)
	SHADER_PARAMETER(float, LocalExposureShadowContrastScale)
	SHADER_PARAMETER(float, LocalExposureDetailStrength)
	SHADER_PARAMETER(float, LocalExposureBlurredLuminanceBlend)
	SHADER_PARAMETER(float, LocalExposureMiddleGreyExposureCompensation)
	SHADER_PARAMETER(float, BlackHistogramBucketInfluence)
	SHADER_PARAMETER(float, GreyMult)
	SHADER_PARAMETER(float, ExponentialUpM)
	SHADER_PARAMETER(float, ExponentialDownM)
	SHADER_PARAMETER(float, StartDistance)
	SHADER_PARAMETER(float, LuminanceMax)
	SHADER_PARAMETER(float, ForceTarget)
	SHADER_PARAMETER(int, VisualizeDebugType)
	SHADER_PARAMETER_TEXTURE(Texture2D, MeterMaskTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, MeterMaskSampler)
END_SHADER_PARAMETER_STRUCT()

FEyeAdaptationParameters GetEyeAdaptationParameters(const FViewInfo& ViewInfo, ERHIFeatureLevel::Type MinFeatureLevel);

// Computes the a fixed exposure to be used to replace the dynamic exposure when it's not supported (< SM5).
float GetEyeAdaptationFixedExposure(const FViewInfo& View);

// Returns the updated 1x1 eye adaptation texture.
FRDGTextureRef AddHistogramEyeAdaptationPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FEyeAdaptationParameters& EyeAdaptationParameters,
	FRDGTextureRef HistogramTexture);

// Computes luma of scene color stores in Alpha.
FScreenPassTexture AddBasicEyeAdaptationSetupPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FEyeAdaptationParameters& EyeAdaptationParameters,
	FScreenPassTexture SceneColor);

// Returns the updated 1x1 eye adaptation texture.
FRDGTextureRef AddBasicEyeAdaptationPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FEyeAdaptationParameters& EyeAdaptationParameters,
	FScreenPassTexture SceneColor,
	FRDGTextureRef EyeAdaptationTexture);