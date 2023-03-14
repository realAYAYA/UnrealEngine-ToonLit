// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImageCoreUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogImageCoreUtils, Log, All);


EPixelFormat FImageCoreUtils::GetPixelFormatForRawImageFormat(ERawImageFormat::Type InFormat, 
							ERawImageFormat::Type * pOutEquivalentFormat)
{
	ERawImageFormat::Type OutEquivalentFormatDummy;
	if ( pOutEquivalentFormat == nullptr )
	{
		pOutEquivalentFormat = &OutEquivalentFormatDummy;
	}
		
	// if *pOutEquivalentFormat != InFormat , then conversion is needed
	*pOutEquivalentFormat = InFormat;

	// do not map to the very closest EPixelFormat
	//	instead map to a close one that is actually usable as Texture

	switch(InFormat)
	{
	case ERawImageFormat::G8:    
		return PF_G8;
	case ERawImageFormat::BGRA8: 
		return PF_B8G8R8A8;
	case ERawImageFormat::BGRE8: 
		*pOutEquivalentFormat = ERawImageFormat::RGBA16F;
		return PF_FloatRGBA;
	case ERawImageFormat::RGBA16:  
		return PF_R16G16B16A16_UNORM;
	case ERawImageFormat::G16: 
		return PF_G16;
	case ERawImageFormat::RGBA16F: 
		return PF_FloatRGBA;
	case ERawImageFormat::RGBA32F: 
		*pOutEquivalentFormat = ERawImageFormat::RGBA16F;
		return PF_FloatRGBA;
	case ERawImageFormat::R16F:	
		return PF_R16F;
	case ERawImageFormat::R32F:	
		return PF_R32_FLOAT; // only if bSupportFilteredFloat32Textures
	
	default:
		check(0);
		return PF_Unknown;
	}
}

	// ETextureSourceFormat and ERawImageFormat::Type are one-to-one :
IMAGECORE_API ERawImageFormat::Type FImageCoreUtils::ConvertToRawImageFormat(ETextureSourceFormat Format)
{
	switch(Format)
	{
	case TSF_G8: return ERawImageFormat::G8;
	case TSF_BGRA8: return ERawImageFormat::BGRA8;
	case TSF_BGRE8: return ERawImageFormat::BGRE8;
	case TSF_RGBA16: return ERawImageFormat::RGBA16;
	case TSF_RGBA16F: return ERawImageFormat::RGBA16F;

	case TSF_G16: return ERawImageFormat::G16;
	case TSF_RGBA32F: return ERawImageFormat::RGBA32F;
	case TSF_R16F: return ERawImageFormat::R16F;
	case TSF_R32F: return ERawImageFormat::R32F;

	// these are mapped to TSF_BGRA8/TSF_BGRE8 on load, so the runtime will never see them :
	case TSF_RGBA8_DEPRECATED:
	case TSF_RGBE8_DEPRECATED:
		UE_LOG(LogImageCoreUtils, Warning,TEXT("Deprecated format in ConvertToRawImageFormat not supported."));
		return ERawImageFormat::Invalid;

	default:
	case TSF_Invalid:
		check(0);
		return ERawImageFormat::Invalid;
	}
}

IMAGECORE_API ETextureSourceFormat FImageCoreUtils::ConvertToTextureSourceFormat(ERawImageFormat::Type Format)
{
	switch(Format)
	{
	case ERawImageFormat::G8:    return TSF_G8;
	case ERawImageFormat::BGRA8: return TSF_BGRA8;
	case ERawImageFormat::BGRE8: return TSF_BGRE8;
	case ERawImageFormat::RGBA16:  return TSF_RGBA16;
	case ERawImageFormat::RGBA16F: return TSF_RGBA16F;
	case ERawImageFormat::RGBA32F: return TSF_RGBA32F;
	case ERawImageFormat::G16: return TSF_G16;
	case ERawImageFormat::R16F:	return TSF_R16F;
	case ERawImageFormat::R32F:	return TSF_R32F;
	
	default:
		check(0);
		return TSF_Invalid;
	}
}


IMAGECORE_API FName FImageCoreUtils::ConvertToUncompressedTextureFormatName(ERawImageFormat::Type Format)
{
// from TextureFormatUncompressed.cpp :
#define ENUM_SUPPORTED_FORMATS(op) \
	op(BGRA8) \
	op(G8) \
	op(G16) \
	op(RGBA16F) \
	op(RGBA32F) \
	op(R16F) \
	op(R32F)
#define DECL_FORMAT_NAME(FormatName) static FName TextureFormatName##FormatName = FName(TEXT(#FormatName));
ENUM_SUPPORTED_FORMATS(DECL_FORMAT_NAME);
#undef DECL_FORMAT_NAME

	switch(Format)
	{
	case ERawImageFormat::G8:    return TextureFormatNameG8;
	case ERawImageFormat::BGRA8: return TextureFormatNameBGRA8;
	case ERawImageFormat::BGRE8: return TextureFormatNameRGBA16F; // <- not same
	case ERawImageFormat::RGBA16:  return TextureFormatNameBGRA8; // <- not same
	case ERawImageFormat::RGBA16F: return TextureFormatNameRGBA16F;
	case ERawImageFormat::RGBA32F: return TextureFormatNameRGBA32F;
	case ERawImageFormat::G16: return TextureFormatNameG16;
	case ERawImageFormat::R16F:	return TextureFormatNameR16F;
	case ERawImageFormat::R32F:	return TextureFormatNameR32F;
	
	default:
		check(0);
		return FName();
	}
}

IMAGECORE_API ETextureSourceFormat FImageCoreUtils::GetCommonSourceFormat(ETextureSourceFormat Format1, ETextureSourceFormat Format2)
{
	if (Format1 == Format2)
	{
		return Format1;
	}

	// calculation is based on the following oriented graph, where formats A and B are linked if conversion from A to B is considered to be acceptable, specifically:
	// TSF_G8 -> TSF_BGRA8 -> TSF_RGBA16
	// TSF_G8 -> TSF_G16 -> TSF_RGBA16
	// TSF_RGBA16 -> TSF_RGBA32F
	// TSF_R16F -> TSF_RGBA16F -> TSF_RGBA32F
	// TSF_R16F -> TSF_R32F -> TSF_RGBA32F
	// TSF_BGRE8 -> TSF_RGBA32F
	// the function returns the lowest possible parent node for two input nodes

	switch (Format1)
	{
	case TSF_G8:
		return !UE::TextureDefines::IsHDR(Format2) ? Format2 : TSF_RGBA32F;
	case TSF_G16:
	case TSF_BGRA8:
		return Format2 == TSF_G8 ? Format1 : !UE::TextureDefines::IsHDR(Format2) ? TSF_RGBA16 : TSF_RGBA32F;
	case TSF_RGBA16:
		return !UE::TextureDefines::IsHDR(Format2) ? Format1 : TSF_RGBA32F;
	case TSF_R16F:
		return Format2 == TSF_R32F || Format2 == TSF_RGBA16F ? Format2 : TSF_RGBA32F;
	case TSF_R32F:
	case TSF_RGBA16F:
		return Format2 == TSF_R16F ? Format1 : TSF_RGBA32F;
	default:
		return TSF_RGBA32F;
	}
}
