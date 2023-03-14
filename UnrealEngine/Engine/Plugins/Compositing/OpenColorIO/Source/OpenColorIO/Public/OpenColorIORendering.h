// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "OpenColorIOColorSpace.h"


class UTexture;
class UTextureRenderTarget2D;
class UWorld;


/** Entry point to trigger OpenColorIO conversion rendering */
class OPENCOLORIO_API FOpenColorIORendering
{
public:

	/**
	 * Applies the color transform described in the settings
	 *
	 * @param InWorld World from which to get the actual shader feature level we need to render
	 * @param InSettings Settings describing the color space transform to apply
	 * @param InTexture Texture in the source color space
	 * @param OutRenderTarget RenderTarget where to draw the input texture in the destination color space
	 * @return True if a rendering command to apply the transform was queued.
	 */
	static bool ApplyColorTransform(UWorld* InWorld, const FOpenColorIOColorConversionSettings& InSettings, UTexture* InTexture, UTextureRenderTarget2D* OutRenderTarget);

private:
	FOpenColorIORendering() {}
};