// Copyright Epic Games, Inc. All Rights Reserved.

#include "LightmapGBuffer.h"

IMPLEMENT_STATIC_UNIFORM_BUFFER_STRUCT(FLightmapGBufferParams, "LightmapGBufferParams", SceneTextures);

IMPLEMENT_MATERIAL_SHADER_TYPE(, FLightmapGBufferVS, TEXT("/Plugin/GPULightmass/Private/LightmapGBuffer.usf"), TEXT("LightmapGBufferVS"), SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(, FLightmapGBufferPS, TEXT("/Plugin/GPULightmass/Private/LightmapGBuffer.usf"), TEXT("LightmapGBufferPS"), SF_Pixel);
