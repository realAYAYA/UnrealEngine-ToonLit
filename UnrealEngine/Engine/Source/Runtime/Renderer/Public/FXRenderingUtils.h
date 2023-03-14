// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FSceneViewFamily;
class FPrimitiveSceneProxy;
class FMaterial;

/**
 * This class exposes methods required by FX rendering that must access rendering internals.
 */ 
class RENDERER_API FFXRenderingUtils
{
public:
	FFXRenderingUtils() = delete;
	FFXRenderingUtils(const FFXRenderingUtils&) = delete;
	FFXRenderingUtils& operator=(const FFXRenderingUtils&) = delete;

	/** Utility to determine if a material might render before the FXSystem's PostRenderOpaque is called for the view family */
	static bool CanMaterialRenderBeforeFXPostOpaque(
		const FSceneViewFamily& ViewFamily,
		const FPrimitiveSceneProxy& SceneProxy,
		const FMaterial& Material);
};
