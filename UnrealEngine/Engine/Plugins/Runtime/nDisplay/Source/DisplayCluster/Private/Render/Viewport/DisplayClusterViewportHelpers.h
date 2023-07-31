// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "PixelFormat.h"

class FDisplayClusterViewportTextureResource;
struct FDisplayClusterRenderFrameSettings;

class DisplayClusterViewportHelpers
{
public:
	// Control texture resources dimensions inside nDisplay
	static int32 GetMinTextureDimension();
	static int32 GetMaxTextureDimension();

	// Texture size helpers
	static bool IsValidTextureSize(const FIntPoint& InSize);

	static FIntPoint ScaleTextureSize(const FIntPoint& InSize, float InMult);

	// Return size less than max texture dimension. Aspect ratio not changed
	static FIntPoint GetTextureSizeLessThanMax(const FIntPoint& InSize, const int32 InMaxTextureDimension);

	static float GetValidSizeMultiplier(const FIntPoint& InSize, const float InSizeMult, const float InBaseSizeMult);

	// Support render freeze feature
	static void FreezeRenderingOfViewportTextureResources(TArray<FDisplayClusterViewportTextureResource*>& TextureResources);

	static int32 GetMaxTextureNumMips(const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, const int32 InNumMips);

#if WITH_EDITOR
	static void GetPreviewRenderTargetDesc_Editor(const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, EPixelFormat& OutPixelFormat, float& OutDisplayGamma, bool& bOutSRGB);
#endif
};
