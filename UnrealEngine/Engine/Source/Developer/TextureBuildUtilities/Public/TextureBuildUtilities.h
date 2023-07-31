// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImageCore.h"
#include "TextureCompressorModule.h" // for FTextureBuildSettings
#include "Interfaces/ITextureFormat.h"

/***

TextureBuildUtilities is a utility module for shared code that Engine and TextureBuildWorker (no Engine) can both see

for Texture-related functions that don't need Texture.h/UTexture

TextureBuildUtilities is hard-linked to the Engine so no LoadModule/ModuleInterface is needed

***/

namespace UE
{
namespace TextureBuildUtilities
{

namespace EncodedTextureExtendedData
{
	TEXTUREBUILDUTILITIES_API FCbObject ToCompactBinary(const FEncodedTextureExtendedData& InExtendedData);
	TEXTUREBUILDUTILITIES_API bool FromCompactBinary(FEncodedTextureExtendedData& OutExtendedData, FCbObject InCbObject);
}

namespace EncodedTextureDescription
{
	TEXTUREBUILDUTILITIES_API FCbObject ToCompactBinary(const FEncodedTextureDescription& InDescription);
	TEXTUREBUILDUTILITIES_API bool FromCompactBinary(FEncodedTextureDescription& OutDescription, FCbObject InCbObject);
}

namespace TextureEngineParameters
{
	TEXTUREBUILDUTILITIES_API FCbObject ToCompactBinaryWithDefaults(const FTextureEngineParameters& InEngineParameters);
	TEXTUREBUILDUTILITIES_API bool FromCompactBinary(FTextureEngineParameters& OutEngineParameters, FCbObject InCbObject);
}

TEXTUREBUILDUTILITIES_API bool TextureFormatIsHdr(FName const& InName);

// Removes platform and other custom prefixes from the name.
// Returns plain format name and the non-platform prefix (with trailing underscore).
// i.e. PLAT_BLAH_AutoDXT returns AutoDXT and writes BLAH_ to OutPrefix.
TEXTUREBUILDUTILITIES_API const FName TextureFormatRemovePrefixFromName(FName const& InName, FName& OutPrefix);

FORCEINLINE const FName TextureFormatRemovePrefixFromName(FName const& InName)
{
	FName OutPrefix;
	return TextureFormatRemovePrefixFromName(InName,OutPrefix);
}

TEXTUREBUILDUTILITIES_API ERawImageFormat::Type GetVirtualTextureBuildIntermediateFormat(const FTextureBuildSettings& BuildSettings);

} // namespace TextureBuildUtilities
} // namespace UE
