// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureBuildUtilities.h"
#include "Misc/DataDrivenPlatformInfoRegistry.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinaryWriter.h"


namespace UE
{
namespace TextureBuildUtilities
{

// Return true if texture format name is HDR
TEXTUREBUILDUTILITIES_API bool TextureFormatIsHdr(FName const& InName)
{
	// TextureFormatRemovePrefixFromName first !
	
	static FName NameRGBA16F(TEXT("RGBA16F"));
	static FName NameRGBA32F(TEXT("RGBA32F"));
	static FName NameR16F(TEXT("R16F"));
	static FName NameR32F(TEXT("R32F"));
	static FName NameBC6H(TEXT("BC6H"));

	if ( InName == NameRGBA16F ) return true;
	if ( InName == NameRGBA32F ) return true;
	if ( InName == NameR16F ) return true;
	if ( InName == NameR32F ) return true;
	if ( InName == NameBC6H ) return true;

	return false;
}

TEXTUREBUILDUTILITIES_API const FName TextureFormatRemovePrefixFromName(FName const& InName, FName& OutPrefix)
{
	FString NameString = InName.ToString();

	// Format names may have one of the following forms:
	// - PLATFORM_PREFIX_FORMAT
	// - PLATFORM_FORMAT
	// - PREFIX_FORMAT
	// - FORMAT
	// We have to remove the platform prefix first, if it exists.
	// Then we detect a non-platform prefix (such as codec name)
	// and split the result into  explicit FORMAT and PREFIX parts.

	for (FName PlatformName : FDataDrivenPlatformInfoRegistry::GetSortedPlatformNames(EPlatformInfoType::AllPlatformInfos))
	{
		FString PlatformTextureFormatPrefix = PlatformName.ToString();
		PlatformTextureFormatPrefix += TEXT('_');
		if (NameString.StartsWith(PlatformTextureFormatPrefix, ESearchCase::IgnoreCase))
		{
			// Remove platform prefix and proceed with non-platform prefix detection.
			NameString = NameString.RightChop(PlatformTextureFormatPrefix.Len());
			break;
		}
	}

	int32 UnderscoreIndex = INDEX_NONE;
	if (NameString.FindChar(TCHAR('_'), UnderscoreIndex))
	{
		// Non-platform prefix, we want to keep these
		OutPrefix = *NameString.Left(UnderscoreIndex + 1);
		return *NameString.RightChop(UnderscoreIndex + 1);
	}

	return *NameString;
}


TEXTUREBUILDUTILITIES_API ERawImageFormat::Type GetVirtualTextureBuildIntermediateFormat(const FTextureBuildSettings& BuildSettings)
{
	const FName TextureFormatName = TextureFormatRemovePrefixFromName(BuildSettings.TextureFormatName);

	// note: using RGBA16F when the Source is HDR but the output is not HDR is not needed
	//	you could use BGRA8 intermediate in that case
	//	but it's rare and not a big problem, so leave it alone for now

	const bool bIsHdr = BuildSettings.bHDRSource || TextureFormatIsHdr(TextureFormatName);

	if (bIsHdr)
	{
		return ERawImageFormat::RGBA16F;
	}
	else if ( TextureFormatName == "G16" )
	{
		return ERawImageFormat::G16;
	}
	else
	{
		return ERawImageFormat::BGRA8;
	}
}

#ifndef TEXT_TO_ENUM
#define TEXT_TO_ENUM(eVal, txt)		if (txt.Compare(#eVal) == 0)	return eVal;
#endif

static EPixelFormat GetPixelFormatFromUtf8(const FUtf8StringView& InPixelFormatStr)
{
#define TEXT_TO_PIXELFORMAT(f) TEXT_TO_ENUM(f, InPixelFormatStr);
	FOREACH_ENUM_EPIXELFORMAT(TEXT_TO_PIXELFORMAT)
#undef TEXT_TO_PIXELFORMAT
		return PF_Unknown;
}


namespace EncodedTextureExtendedData
{
	FCbObject ToCompactBinary(const FEncodedTextureExtendedData& InExtendedData)
	{
		FCbWriter Writer;
		Writer.BeginObject();
		Writer.AddInteger("NumMipsInTail", InExtendedData.NumMipsInTail);
		Writer.AddInteger("ExtData", InExtendedData.ExtData);
		Writer.BeginArray("MipSizes");
		for (uint64 MipSize : InExtendedData.MipSizesInBytes)
		{
			Writer.AddInteger(MipSize);
		}
		Writer.EndArray();
		Writer.EndObject();
		return Writer.Save().AsObject();
	}

	bool FromCompactBinary(FEncodedTextureExtendedData& OutExtendedData, FCbObject InCbObject)
	{
		OutExtendedData.ExtData = InCbObject["ExtData"].AsUInt32();
		OutExtendedData.NumMipsInTail = InCbObject["NumMipsInTail"].AsInt32();

		FCbArrayView MipArrayView = InCbObject["MipSizes"].AsArrayView();
		for (FCbFieldView MipFieldView : MipArrayView)
		{
			OutExtendedData.MipSizesInBytes.Add(MipFieldView.AsUInt64());
		}
		return true;
	}
} // namespace EncodedTextureExtendedData

namespace EncodedTextureDescription
{
	FCbObject ToCompactBinary(const FEncodedTextureDescription& InDescription)
	{
		FCbWriter Writer;
		Writer.BeginObject();
		Writer.AddInteger("TopMipSizeX", InDescription.TopMipSizeX);
		Writer.AddInteger("TopMipSizeY", InDescription.TopMipSizeY);
		Writer.AddInteger("TopMipVolumeSizeZ", InDescription.TopMipVolumeSizeZ);
		Writer.AddInteger("ArraySlices", InDescription.ArraySlices);
		Writer.AddString("PixelFormat", GetPixelFormatString(InDescription.PixelFormat));
		Writer.AddInteger("NumMips", InDescription.NumMips);
		Writer.AddBool("bCubeMap", InDescription.bCubeMap);
		Writer.AddBool("bTextureArray", InDescription.bTextureArray);
		Writer.AddBool("bVolumeTexture", InDescription.bVolumeTexture);
		Writer.EndObject();
		return Writer.Save().AsObject();
	}

	bool FromCompactBinary(FEncodedTextureDescription& OutDescription, FCbObject InCbObject)
	{
		OutDescription.TopMipSizeX = InCbObject["TopMipSizeX"].AsInt32();
		OutDescription.TopMipSizeY = InCbObject["TopMipSizeY"].AsInt32();
		OutDescription.TopMipVolumeSizeZ = InCbObject["TopMipVolumeSizeZ"].AsInt32();
		OutDescription.ArraySlices = InCbObject["ArraySlices"].AsInt32();
		OutDescription.PixelFormat = GetPixelFormatFromUtf8(InCbObject["PixelFormat"].AsString());
		OutDescription.NumMips = (uint8)InCbObject["NumMips"].AsInt32();
		OutDescription.bCubeMap = InCbObject["bCubeMap"].AsBool();
		OutDescription.bTextureArray = InCbObject["bTextureArray"].AsBool();
		OutDescription.bVolumeTexture = InCbObject["bVolumeTexture"].AsBool();
		return true;
	}
} // namespace EncodedTextureDescription



namespace TextureEngineParameters
{
	FCbObject ToCompactBinaryWithDefaults(const FTextureEngineParameters& InEngineParameters)
	{
		FTextureEngineParameters Defaults;

		FCbWriter Writer;
		Writer.BeginObject();
		if (InEngineParameters.bEngineSupportsTexture2DArrayStreaming != Defaults.bEngineSupportsTexture2DArrayStreaming)
		{
			Writer.AddBool("bEngineSupportsTexture2DArrayStreaming", InEngineParameters.bEngineSupportsTexture2DArrayStreaming);
		}
		if (InEngineParameters.bEngineSupportsVolumeTextureStreaming != Defaults.bEngineSupportsVolumeTextureStreaming)
		{
			Writer.AddBool("bEngineSupportsVolumeTextureStreaming", InEngineParameters.bEngineSupportsVolumeTextureStreaming);
		}
		if (InEngineParameters.NumInlineDerivedMips != Defaults.NumInlineDerivedMips)
		{
			Writer.AddInteger("NumInlineDerivedMips", InEngineParameters.NumInlineDerivedMips);
		}
		Writer.EndObject();
		return Writer.Save().AsObject();
	}

	bool FromCompactBinary(FTextureEngineParameters& OutEngineParameters, FCbObject InCbObject)
	{
		OutEngineParameters = FTextureEngineParameters(); // init to defaults

		OutEngineParameters.NumInlineDerivedMips = InCbObject["NumInlineDerivedMips"].AsInt32(OutEngineParameters.NumInlineDerivedMips);
		OutEngineParameters.bEngineSupportsTexture2DArrayStreaming = InCbObject["bEngineSupportsTexture2DArrayStreaming"].AsBool(OutEngineParameters.bEngineSupportsTexture2DArrayStreaming);
		OutEngineParameters.bEngineSupportsVolumeTextureStreaming = InCbObject["bEngineSupportsVolumeTextureStreaming"].AsBool(OutEngineParameters.bEngineSupportsVolumeTextureStreaming);
		return true;
	}
} // namespace EncodedTextureDescription



} // namespace
}