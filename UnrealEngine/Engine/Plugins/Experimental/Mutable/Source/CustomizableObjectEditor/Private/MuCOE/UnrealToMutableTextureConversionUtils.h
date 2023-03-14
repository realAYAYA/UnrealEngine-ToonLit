// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/Tuple.h"

#include "MuR/Image.h"

// Forward declarations
class UTexture2D;

enum class EUnrealToMutableConversionError 
{
    Success,
    UnsupportedFormat,
    CompositeImageDimensionMissmatch,
    CompositeUnsupportedFormat,
    Unknown
};

TTuple<mu::ImagePtr, EUnrealToMutableConversionError> ConvertTextureUnrealToMutable(UTexture2D* Texture, bool bIsNormalComposite = false );
