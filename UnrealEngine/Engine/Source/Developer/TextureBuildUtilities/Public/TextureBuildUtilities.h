// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImageCore.h"
#include "Interfaces/ITextureFormat.h"

struct FTextureBuildSettings;

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

// Carries information out of the build that we don't want to cook or save off in the runtime
struct TEXTUREBUILDUTILITIES_API FTextureBuildMetadata
{
	// Whether or not the first mip level of the source has alpha detected. This is used e.g.
	// picking between BC1 and BC3
	bool bSourceMipsAlphaDetected = false;
	
	// Digests of the data at various processing stages so we can track down determinism issues
	// that arise.
	uint64 PreEncodeMipsHash = 0;

	FCbObject ToCompactBinaryWithDefaults() const;
	FTextureBuildMetadata(FCbObject InCbObject);
	FTextureBuildMetadata() = default;
};

TEXTUREBUILDUTILITIES_API bool TextureFormatIsHdr(FName const& InName);

// Removes platform and other custom prefixes from the name.
// Returns plain format name and the non-platform prefix (with trailing underscore).
// i.e. PLAT_BLAH_AutoDXT returns AutoDXT and writes BLAH_ to OutPrefix.
TEXTUREBUILDUTILITIES_API const FName TextureFormatRemovePrefixFromName(FName const& InName, FName& OutPrefix);

// removes platform prefix but leaves other custom prefixes :
TEXTUREBUILDUTILITIES_API const FName TextureFormatRemovePlatformPrefixFromName(FName const& InName);

FORCEINLINE const FName TextureFormatRemovePrefixFromName(FName const& InName)
{
	FName OutPrefix;
	return TextureFormatRemovePrefixFromName(InName,OutPrefix);
}

// Get the format to use for output of the VT Intermediate stage, cutting into tiles and processing
//	  the next step will then encode from this format to the desired output format
TEXTUREBUILDUTILITIES_API ERawImageFormat::Type GetVirtualTextureBuildIntermediateFormat(const FTextureBuildSettings& BuildSettings);

} // namespace TextureBuildUtilities
} // namespace UE
