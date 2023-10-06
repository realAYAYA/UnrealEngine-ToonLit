// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScreenPass.h"

BEGIN_SHADER_PARAMETER_STRUCT(FColorRemapParameters, )
	SHADER_PARAMETER(FVector3f, MappingPolynomial)
END_SHADER_PARAMETER_STRUCT()

FColorRemapParameters GetColorRemapParameters();

bool PipelineVolumeTextureLUTSupportGuaranteedAtRuntime(EShaderPlatform Platform);

FRDGTextureRef AddCombineLUTPass(FRDGBuilder& GraphBuilder, const FViewInfo& View);