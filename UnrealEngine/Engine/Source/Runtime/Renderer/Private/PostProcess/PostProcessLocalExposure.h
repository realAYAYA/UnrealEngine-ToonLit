// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScreenPass.h"

class FEyeAdaptationParameters;

BEGIN_SHADER_PARAMETER_STRUCT(FLocalExposureParameters, )
	SHADER_PARAMETER(float, HighlightContrastScale)
	SHADER_PARAMETER(float, ShadowContrastScale)
	SHADER_PARAMETER(float, DetailStrength)
	SHADER_PARAMETER(float, BlurredLuminanceBlend)
	SHADER_PARAMETER(float, MiddleGreyExposureCompensation)
	SHADER_PARAMETER(FVector2f, BilateralGridUVScale)
END_SHADER_PARAMETER_STRUCT()

FLocalExposureParameters GetLocalExposureParameters(const FViewInfo& View, FIntPoint ViewRectSize, const FEyeAdaptationParameters& EyeAdaptationParameters);

FRDGTextureRef AddLocalExposureBlurredLogLuminancePass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FEyeAdaptationParameters& EyeAdaptationParameters,
	FScreenPassTexture InputTexture);

void AddApplyLocalExposurePass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FEyeAdaptationParameters& EyeAdaptationParameters,
	FRDGBufferRef EyeAdaptationBuffer,
	const FLocalExposureParameters& LocalExposureParamaters,
	FRDGTextureRef LocalExposureTexture,
	FRDGTextureRef BlurredLogLuminanceTexture,
	FScreenPassTexture Input,
	FScreenPassTexture Output,
	ERDGPassFlags PassFlags);
