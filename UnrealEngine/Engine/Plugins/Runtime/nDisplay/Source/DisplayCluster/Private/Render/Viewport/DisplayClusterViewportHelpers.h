// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "PixelFormat.h"

class FDisplayClusterViewportResource;
struct FDisplayClusterRenderFrameSettings;

/**
 * Helper class for DC viewports
 */
class FDisplayClusterViewportHelpers
{
public:
	/** Get the maximum allowable texture size used for the nDisplay viewport. */
	static int32 GetMinTextureDimension();

	/** Get the minimum allowable texture size used for the nDisplay viewport. */
	static int32 GetMaxTextureDimension();

	/** Get the valid viewport size.
	* 
	* @param InRect         - source region
	* @param InViewportId   - The viewport name (debug purpose)
	* @param InResourceName - The resource name (debug purpose)
	* 
	* @return the region of the viewport that can be used
	*/
	static FIntRect GetValidViewportRect(const FIntRect& InRect, const FString& InViewportId, const TCHAR* InResourceName = nullptr);

	/** Return true, if size is valid.
	* 
	* @param InSize - the size to be checked.
	* 
	* @return true, if the size is within the minimum and maximum dimensions
	*/
	static bool IsValidTextureSize(const FIntPoint& InSize);

	/** Scaling texture size with a multiplier
	* 
	* @param InSize - the input texture size
	* @param InMult - texture size multiplier
	* 
	* @return - scaled texture size
	*/
	static FIntPoint ScaleTextureSize(const FIntPoint& InSize, float InMult);

	/** The returned size is smaller than the maximum texture size. The aspect ratio does not change
	* 
	* @param InSize - input texture size
	* @param InMaxTextureDimension - max alloable dimension
	* 
	* @return - the same as the input size, or reduced to the InMaxTextureDimension
	 */
	static FIntPoint GetTextureSizeLessThanMax(const FIntPoint& InSize, const int32 InMaxTextureDimension);

	/** Find an acceptable multiplier for the texture size
	* 
	* @param InSize - in texture size
	* @param InSizeMult - the target multiplier
	* @param InBaseSizeMult - the base multiplier
	* 
	* @return - a multiplier that gives the valid size of the texture
	*/
	static float GetValidSizeMultiplier(const FIntPoint& InSize, const float InSizeMult, const float InBaseSizeMult);

	/** Getting the maximum allowable mips value for the rendering frame settings
	* 
	* @param InRenderFrameSettings  -the current render frame settings
	* @param InNumMips - texture mips value from the settings
	* 
	* @return - the mips value to be used.
	*/
	static int32 GetMaxTextureNumMips(const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, const int32 InNumMips);

	/**
	 * Getting the default pixel format for preview rendering
	 */
	static EPixelFormat GetPreviewDefaultPixelFormat();

	/**
	 * Getting the default pixel format
	 */
	static EPixelFormat GetDefaultPixelFormat();
};
