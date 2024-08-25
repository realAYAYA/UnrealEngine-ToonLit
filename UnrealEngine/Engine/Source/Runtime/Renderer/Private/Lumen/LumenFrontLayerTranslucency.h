// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "RenderGraphResources.h"

class FLumenFrontLayerTranslucency
{
public:
	FRDGTextureRef Radiance = nullptr;
	FRDGTextureRef Normal = nullptr;
	FRDGTextureRef SceneDepth = nullptr;
	bool bEnabled = false;
	float RelativeDepthThreshold = 0.0f;
};

struct FFrontLayerTranslucencyData
{
	bool IsValid() const { return SceneDepth != nullptr; }
	FRDGTextureRef Normal = nullptr;
	FRDGTextureRef SceneDepth = nullptr;
};

BEGIN_SHADER_PARAMETER_STRUCT(FLumenFrontLayerTranslucencyGBufferParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FrontLayerTranslucencyNormal)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FrontLayerTranslucencySceneDepth)
END_SHADER_PARAMETER_STRUCT()

// Used by Translucency Base Pass
BEGIN_SHADER_PARAMETER_STRUCT(FLumenFrontLayerTranslucencyReflectionParameters, )
    SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, Radiance)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Normal)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepth)
	SHADER_PARAMETER(uint32, Enabled)
	SHADER_PARAMETER(float, RelativeDepthThreshold)
	SHADER_PARAMETER(float, SpecularScale)
	SHADER_PARAMETER(float, Contrast)
END_SHADER_PARAMETER_STRUCT()
