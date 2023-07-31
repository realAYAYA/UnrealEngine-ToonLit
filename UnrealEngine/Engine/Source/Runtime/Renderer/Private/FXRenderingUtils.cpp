// Copyright Epic Games, Inc. All Rights Reserved.

#include "FXRenderingUtils.h"
#include "MaterialShared.h"
#include "Lumen/LumenScreenProbeGather.h"

bool FFXRenderingUtils::CanMaterialRenderBeforeFXPostOpaque(
	const FSceneViewFamily& ViewFamily,
	const FPrimitiveSceneProxy& SceneProxy,
	const FMaterial& Material)
{
	const EBlendMode BlendMode = Material.GetBlendMode();
	const bool bTranslucent = IsTranslucentBlendMode(BlendMode);		

	if (!bTranslucent)
	{
		// opaque materials always render before FFXSystemInterface::PostOpaqueRender
		return true;
	}

	// When rendering Lumen, it's possible a translucent material might render in the LumenTranslucencyRadianceCacheMark pass,
	// which happens before PostRenderOpaque.
	const FScene* Scene = SceneProxy.GetScene().GetRenderScene();
	if (Scene 
		&& (CanMaterialRenderInLumenTranslucencyRadianceCacheMarkPass(*Scene, ViewFamily, SceneProxy, Material)
			|| CanMaterialRenderInLumenFrontLayerTranslucencyGBufferPass(*Scene, ViewFamily, SceneProxy, Material)))
	{
		return true;
	}
		
	return false;
}