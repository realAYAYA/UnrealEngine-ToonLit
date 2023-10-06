// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "CoreTypes.h"

class UTexture;
class ITargetPlatform;

// Not tested against all the texture types and the api might change or be removed without deprecation
namespace UE::TextureUtilitiesCommon::Experimental
{
	// Try to resize the texture source data so that it is not larger than one build for the specified platform (only Texture2D and TextureCube are supported at the moment)
	// Note it does not trigger the post edit change after modifying the source texture
	TEXTUREUTILITIESCOMMON_API bool DownsizeTextureSourceData(UTexture* Texture, int32 TargetSizeInGame, const ITargetPlatform* TargetPlatform);

	//Note: This function does trigger the post edit change after modifying the source texture
	TEXTUREUTILITIESCOMMON_API bool DownsizeTexureSourceDataNearRenderingSize(UTexture* Texture, const ITargetPlatform* TargetPlatform);
}

#endif // WITH_EDITOR