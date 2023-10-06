// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Engine/TextureDefines.h"

struct FImage;

namespace UE::TextureUtilitiesCommon
{
	/**
	 * Detect the existence of gray scale image in some formats and convert those to a gray scale equivalent image
	 * 
	 * @return true if the image was converted
	 */
	TEXTUREUTILITIESCOMMON_API bool AutoDetectAndChangeGrayScale(FImage& Image);

	/**
	 * For PNG texture importing, this ensures that any pixels with an alpha value of zero have an RGB
	 * assigned to them from a neighboring pixel which has non-zero alpha.
	 * This is needed as PNG exporters tend to turn pixels that are RGBA = (x,x,x,0) to (1,1,1,0)
	 * and this produces artifacts when drawing the texture with bilinear filtering. 
	 *
	 * @param TextureSource - The source texture
	 * @param SourceData - The source texture data
	 */
	TEXTUREUTILITIESCOMMON_API void FillZeroAlphaPNGData(int32 SizeX, int32 SizeY, ETextureSourceFormat SourceFormat, uint8* SourceData, bool bDoOnComplexAlphaNotJustBinaryTransparency);
}