// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "HAL/Platform.h"

namespace mu
{
	class Image;
}

// Forward declarations
class UTexture2D;

enum class EUnrealToMutableConversionError 
{
    Success,
    UnsupportedFormat,
    CompositeImageDimensionMismatch,
    CompositeUnsupportedFormat,
    Unknown
};


CUSTOMIZABLEOBJECT_API EUnrealToMutableConversionError ConvertTextureUnrealSourceToMutable(mu::Image* OutResult, UTexture2D* Texture, bool bIsNormalComposite, uint8 MipmapsToSkip);

#endif
