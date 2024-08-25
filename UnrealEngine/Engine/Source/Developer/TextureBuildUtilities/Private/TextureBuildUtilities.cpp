// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureBuildUtilities.h"
#include "TextureCompressorModule.h" // for FTextureBuildSettings
#include "Misc/DataDrivenPlatformInfoRegistry.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinaryWriter.h"
//#include "EngineLogs.h" // can't use from SCW


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

TEXTUREBUILDUTILITIES_API const FName TextureFormatRemovePlatformPrefixFromName(FName const& InName)
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

	// fast(ish) early out if there are no underscores in InName :
	int32 UnderscoreIndexIgnored = INDEX_NONE;
	if ( ! NameString.FindChar(TCHAR('_'), UnderscoreIndexIgnored))
	{
		return InName;
	}

	for (FName PlatformName : FDataDrivenPlatformInfoRegistry::GetSortedPlatformNames(EPlatformInfoType::AllPlatformInfos))
	{
		FString PlatformTextureFormatPrefix = PlatformName.ToString();
		PlatformTextureFormatPrefix += TEXT('_');
		if (NameString.StartsWith(PlatformTextureFormatPrefix, ESearchCase::IgnoreCase))
		{
			// Remove platform prefix and proceed with non-platform prefix detection.
			FString PlatformRemoved = NameString.RightChop(PlatformTextureFormatPrefix.Len());
			return FName( PlatformRemoved );
		}
	}
	
	return InName;
}
	
TEXTUREBUILDUTILITIES_API const FName TextureFormatRemovePrefixFromName(FName const& InNameWithPlatform, FName& OutPrefix)
{
	// first remove platform prefix :
	FName NameWithoutPlatform = TextureFormatRemovePlatformPrefixFromName( InNameWithPlatform );
	FString NameString = NameWithoutPlatform.ToString();

	// then see if there's another underscore separated prefix :
	int32 UnderscoreIndex = INDEX_NONE;
	if ( ! NameString.FindChar(TCHAR('_'), UnderscoreIndex))
	{
		return NameWithoutPlatform;
	}

	// texture format names can have underscores in them (eg. ETC2_RG11)
	//	so need to differentiate between that and a conditional prefix :

	// found an underscore; is it a composite texture name, or an "Alternate" prefix?
	FString Prefix = NameString.Left(UnderscoreIndex + 1);
	if ( Prefix == "OODLE_" || Prefix == "TFO_" )
	{
		// Alternate prefix
		OutPrefix = FName( Prefix );
		return FName( NameString.RightChop(UnderscoreIndex + 1) );
	}
	else if ( Prefix == "ASTC_" || Prefix == "ETC2_" )
	{
		// composite format, don't split
		return NameWithoutPlatform;
	}
	else
	{
		// prefix not recognized
		// LogTexture doesn't exist in SCW
		UE_LOG(LogCore,Warning,TEXT("Texture Format Prefix not recognized: %s [%s]"),*Prefix,*InNameWithPlatform.ToString());
		
		return NameWithoutPlatform;
	}
}


TEXTUREBUILDUTILITIES_API ERawImageFormat::Type GetVirtualTextureBuildIntermediateFormat(const FTextureBuildSettings& BuildSettings)
{
	// Platform prefix should have already been removed, also remove any Oodle prefix:
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

FCbObject FTextureBuildMetadata::ToCompactBinaryWithDefaults() const
{
	FTextureBuildMetadata Defaults;

	FCbWriter Writer;
	Writer.BeginObject();
	if (PreEncodeMipsHash != Defaults.PreEncodeMipsHash)
	{
		Writer << UTF8TEXTVIEW("PreEncodeMipsHash") << PreEncodeMipsHash;
	}
	Writer.EndObject();
	return Writer.Save().AsObject();
}

FTextureBuildMetadata::FTextureBuildMetadata(FCbObject InCbObject)
{
	PreEncodeMipsHash = InCbObject["PreEncodeMipsHash"].AsUInt64(PreEncodeMipsHash);
}

void GetPlaceholderTextureImageInfo(FImageInfo* OutImageInfo)
{
	OutImageInfo->SizeX = 4;
	OutImageInfo->SizeY = 4;
	OutImageInfo->GammaSpace = EGammaSpace::sRGB;
	OutImageInfo->Format = ERawImageFormat::BGRA8;
	OutImageInfo->NumSlices = 1;
}
void GetPlaceholderTextureImage(FImage* OutImage)
{
	*OutImage = FImage();

	GetPlaceholderTextureImageInfo(OutImage);
	OutImage->RawData.AddUninitialized(sizeof(FColor) * OutImage->SizeX * OutImage->SizeY);
	for (FColor& Color : OutImage->AsBGRA8())
	{
		Color = FColor::Black;
	}

}


// Returns true if the target texture size is different and padding/stretching is required.
TEXTUREBUILDUTILITIES_API bool GetPowerOfTwoTargetTextureSize(int32 InMip0SizeX, int32 InMip0SizeY, int32 InMip0NumSlices, bool bInIsVolume, ETexturePowerOfTwoSetting::Type InPow2Setting, int32 InResizeDuringBuildX, int32 InResizeDuringBuildY, int32& OutTargetSizeX, int32& OutTargetSizeY, int32& OutTargetSizeZ)
{
	int32 TargetTextureSizeX = InMip0SizeX;
	int32 TargetTextureSizeY = InMip0SizeY;
	int32 TargetTextureSizeZ = bInIsVolume ? InMip0NumSlices : 1; // Only used for volume texture.

	const int32 PowerOfTwoTextureSizeX = FMath::RoundUpToPowerOfTwo(TargetTextureSizeX);
	const int32 PowerOfTwoTextureSizeY = FMath::RoundUpToPowerOfTwo(TargetTextureSizeY);
	const int32 PowerOfTwoTextureSizeZ = FMath::RoundUpToPowerOfTwo(TargetTextureSizeZ);

	switch (InPow2Setting)
	{
	case ETexturePowerOfTwoSetting::None:
		break;

	case ETexturePowerOfTwoSetting::PadToPowerOfTwo:
	case ETexturePowerOfTwoSetting::StretchToPowerOfTwo:
		TargetTextureSizeX = PowerOfTwoTextureSizeX;
		TargetTextureSizeY = PowerOfTwoTextureSizeY;
		TargetTextureSizeZ = PowerOfTwoTextureSizeZ;
		break;

	case ETexturePowerOfTwoSetting::PadToSquarePowerOfTwo:
	case ETexturePowerOfTwoSetting::StretchToSquarePowerOfTwo:
		TargetTextureSizeX = TargetTextureSizeY = TargetTextureSizeZ =
			FMath::Max3<int32>(PowerOfTwoTextureSizeX, PowerOfTwoTextureSizeY, PowerOfTwoTextureSizeZ);
		break;

	case ETexturePowerOfTwoSetting::ResizeToSpecificResolution:
		if (InResizeDuringBuildX)
		{
			TargetTextureSizeX = InResizeDuringBuildX;
		}
		if (InResizeDuringBuildY)
		{
			TargetTextureSizeY = InResizeDuringBuildY;
		}
		break;

	default:
		checkf(false, TEXT("Unknown entry in ETexturePowerOfTwoSetting::Type"));
		break;
	}

	// Z only matters as a sampling dimension if we are a volume texture.
	if (bInIsVolume == false)
	{
		TargetTextureSizeZ = InMip0NumSlices;
	}

	OutTargetSizeX = TargetTextureSizeX;
	OutTargetSizeY = TargetTextureSizeY;
	OutTargetSizeZ = TargetTextureSizeZ;

	return (TargetTextureSizeX != InMip0SizeX) ||
		(TargetTextureSizeY != InMip0SizeY) ||
		(bInIsVolume && TargetTextureSizeZ != InMip0NumSlices);
}

} // namespace
}