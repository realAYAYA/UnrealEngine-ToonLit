// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ShaderParameterMacros.h"

// this struct holds skylight parameters
BEGIN_SHADER_PARAMETER_STRUCT(FPathTracingSkylight, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SkylightTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SkylightPdf)
	SHADER_PARAMETER_SAMPLER(SamplerState, SkylightTextureSampler)
	SHADER_PARAMETER(float, SkylightInvResolution)
	SHADER_PARAMETER(int32, SkylightMipCount)
END_SHADER_PARAMETER_STRUCT()

class FRDGBuilder;
class FScene;
class FViewInfo;
class FSceneViewFamily;

bool PrepareSkyTexture(FRDGBuilder& GraphBuilder, FScene* Scene, const FViewInfo& View, bool SkylightEnabled, bool UseMISCompensation, FPathTracingSkylight* SkylightParameters);

namespace PathTracing
{
	bool UsesDecals(const FSceneViewFamily& ViewFamily);
}
