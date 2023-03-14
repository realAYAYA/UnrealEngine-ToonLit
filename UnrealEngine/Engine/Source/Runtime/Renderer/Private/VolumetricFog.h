// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VolumetricFog.h
=============================================================================*/

#pragma once

#include "RHIDefinitions.h"
#include "SceneView.h"
#include "SceneRendering.h"


class FTransientLightFunctionTextureAtlas
{
public:

	FTransientLightFunctionTextureAtlas(FRDGBuilder& GraphBuilder);
	~FTransientLightFunctionTextureAtlas();

	// FTransientLightFunctionTextureAtlasTile will never be null, but it can be a default white light function
	FTransientLightFunctionTextureAtlasTile AllocateAtlasTile();

	FRDGTextureRef GetTransientLightFunctionAtlasTexture()
	{
		return TransientLightFunctionAtlasTexture;
	}
	FRDGTextureRef GetDefaultLightFunctionTexture()
	{
		return DefaultLightFunctionAtlasItemTexture;
	}

	uint32 GetAtlasTextureWidth()
	{
		return AtlasTextureWidth;
	}

private:
	FTransientLightFunctionTextureAtlas() {}

	uint32 AtlasItemWidth;
	uint32 AtlasTextureWidth;
	uint32 AllocatedAtlasTiles;
	float HalfTexelSize;

	FRDGTextureRef TransientLightFunctionAtlasTexture;
	FRDGTextureRef DefaultLightFunctionAtlasItemTexture;
};

inline bool DoesPlatformSupportVolumetricFog(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsVolumetricFog(Platform);
}

inline bool DoesPlatformSupportVolumetricFogVoxelization(const FStaticShaderPlatform Platform)
{
	return DoesPlatformSupportVolumetricFog(Platform);
}

extern bool ShouldRenderVolumetricFog(const FScene* Scene, const FSceneViewFamily& ViewFamily);
extern const FProjectedShadowInfo* GetShadowForInjectionIntoVolumetricFog(const FVisibleLightInfo& VisibleLightInfo);

extern bool LightNeedsSeparateInjectionIntoVolumetricFogForOpaqueShadow(const FViewInfo& View, const FLightSceneInfo* LightSceneInfo, const FVisibleLightInfo& VisibleLightInfo);
extern bool LightNeedsSeparateInjectionIntoVolumetricFogForLightFunction(const FLightSceneInfo* LightSceneInfo);
