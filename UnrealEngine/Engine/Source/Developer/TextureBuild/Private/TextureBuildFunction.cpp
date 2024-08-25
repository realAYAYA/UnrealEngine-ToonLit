// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureBuildFunction.h"

#include "DerivedDataCache.h"
#include "DerivedDataValueId.h"
#include "Engine/TextureDefines.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageCore.h"
#include "ImageCoreUtils.h"
#include "Interfaces/ITextureFormat.h"
#include "Interfaces/ITextureFormatModule.h"
#include "IO/IoHash.h"
#include "Memory/CompositeBuffer.h"
#include "Memory/SharedBuffer.h"
#include "Modules/ModuleManager.h"
#include "PixelFormat.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinaryWriter.h"
#include "Serialization/FileRegions.h"
#include "Serialization/MemoryWriter.h"
#include "TextureBuildUtilities.h"
#include "TextureCompressorModule.h"
#include "TextureFormatManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogTextureBuildFunction, Log, All);

// Any edits to the texture compressor or this file that will change the output of texture builds
// MUST have a corresponding change to this version. Individual texture formats have a version to
// change that is specific to the format. A merge conflict affecting the version MUST be resolved
// by generating a new version. This also includes the addition of new outputs to the build, as
// this will cause a DDC verification failure unless a new version is created.
// A reminder that for DDC invalidation, running a ddc fill job or the ddc commandlet is a friendly
// thing to do! -run=DerivedDataCache -Fill -TargetPlatform=Platform1,Platform...N
//
static const FGuid TextureBuildFunctionVersion(TEXT("B20676CE-A786-43EE-96F0-2620A4C38ACA"));

static void ReadCbField(FCbFieldView Field, bool& OutValue) { OutValue = Field.AsBool(OutValue); }
static void ReadCbField(FCbFieldView Field, int32& OutValue) { OutValue = Field.AsInt32(OutValue); }
static void ReadCbField(FCbFieldView Field, uint8& OutValue) { OutValue = Field.AsUInt8(OutValue); }
static void ReadCbField(FCbFieldView Field, uint32& OutValue) { OutValue = Field.AsUInt32(OutValue); }
static void ReadCbField(FCbFieldView Field, float& OutValue) { OutValue = Field.AsFloat(OutValue); }
static void ReadCbField(FCbFieldView Field, FGuid& OutValue) { OutValue = Field.AsUuid(); }

static void ReadCbField(FCbFieldView Field, FName& OutValue)
{
	if (Field.IsString())
	{
		OutValue = FName(FUTF8ToTCHAR(Field.AsString()));
	}
}

static void ReadCbField(FCbFieldView Field, FColor& OutValue)
{
	FCbFieldViewIterator It = Field.AsArrayView().CreateViewIterator();
	OutValue.A = It++->AsUInt8(OutValue.A);
	OutValue.R = It++->AsUInt8(OutValue.R);
	OutValue.G = It++->AsUInt8(OutValue.G);
	OutValue.B = It++->AsUInt8(OutValue.B);
}

static void ReadCbField(FCbFieldView Field, FVector2f& OutValue)
{
	FCbFieldViewIterator It = Field.AsArrayView().CreateViewIterator();
	OutValue.X = It++->AsFloat(OutValue.X);
	OutValue.Y = It++->AsFloat(OutValue.Y);
}

static void ReadCbField(FCbFieldView Field, FVector4f& OutValue)
{
	FCbFieldViewIterator It = Field.AsArrayView().CreateViewIterator();
	OutValue.X = It++->AsFloat(OutValue.X);
	OutValue.Y = It++->AsFloat(OutValue.Y);
	OutValue.Z = It++->AsFloat(OutValue.Z);
	OutValue.W = It++->AsFloat(OutValue.W);
}

static void ReadCbField(FCbFieldView Field, FIntPoint& OutValue)
{
	FCbFieldViewIterator It = Field.AsArrayView().CreateViewIterator();
	OutValue.X = It++->AsInt32(OutValue.X);
	OutValue.Y = It++->AsInt32(OutValue.Y);
}

static FTextureBuildSettings ReadBuildSettingsFromCompactBinary(const FCbObjectView& Object)
{
	FTextureBuildSettings BuildSettings;
	BuildSettings.FormatConfigOverride = Object["FormatConfigOverride"].AsObjectView();
	FCbObjectView ColorAdjustmentCbObj = Object["ColorAdjustment"].AsObjectView();
	FColorAdjustmentParameters& ColorAdjustment = BuildSettings.ColorAdjustment;
	ReadCbField(ColorAdjustmentCbObj["AdjustBrightness"], ColorAdjustment.AdjustBrightness);
	ReadCbField(ColorAdjustmentCbObj["AdjustBrightnessCurve"], ColorAdjustment.AdjustBrightnessCurve);
	ReadCbField(ColorAdjustmentCbObj["AdjustSaturation"], ColorAdjustment.AdjustSaturation);
	ReadCbField(ColorAdjustmentCbObj["AdjustVibrance"], ColorAdjustment.AdjustVibrance);
	ReadCbField(ColorAdjustmentCbObj["AdjustRGBCurve"], ColorAdjustment.AdjustRGBCurve);
	ReadCbField(ColorAdjustmentCbObj["AdjustHue"], ColorAdjustment.AdjustHue);
	ReadCbField(ColorAdjustmentCbObj["AdjustMinAlpha"], ColorAdjustment.AdjustMinAlpha);
	ReadCbField(ColorAdjustmentCbObj["AdjustMaxAlpha"], ColorAdjustment.AdjustMaxAlpha);
	BuildSettings.bUseNewMipFilter = Object["bUseNewMipFilter"].AsBool(BuildSettings.bUseNewMipFilter);
	BuildSettings.bNormalizeNormals = Object["bNormalizeNormals"].AsBool(BuildSettings.bNormalizeNormals);
	BuildSettings.bDoScaleMipsForAlphaCoverage = Object["bDoScaleMipsForAlphaCoverage"].AsBool(BuildSettings.bDoScaleMipsForAlphaCoverage);
	ReadCbField(Object["AlphaCoverageThresholds"], BuildSettings.AlphaCoverageThresholds);
	ReadCbField(Object["MipSharpening"], BuildSettings.MipSharpening);
	ReadCbField(Object["DiffuseConvolveMipLevel"], BuildSettings.DiffuseConvolveMipLevel);
	ReadCbField(Object["SharpenMipKernelSize"], BuildSettings.SharpenMipKernelSize);
	ReadCbField(Object["MaxTextureResolution"], BuildSettings.MaxTextureResolution);
	check( BuildSettings.MaxTextureResolution != 0 );
	ReadCbField(Object["TextureFormatName"], BuildSettings.TextureFormatName);
	ReadCbField(Object["bHDRSource"], BuildSettings.bHDRSource);
	ReadCbField(Object["MipGenSettings"], BuildSettings.MipGenSettings);
	BuildSettings.bCubemap = Object["bCubemap"].AsBool(BuildSettings.bCubemap);
	BuildSettings.bTextureArray = Object["bTextureArray"].AsBool(BuildSettings.bTextureArray);
	BuildSettings.bVolume = Object["bVolume"].AsBool(BuildSettings.bVolume);
	BuildSettings.bLongLatSource = Object["bLongLatSource"].AsBool(BuildSettings.bLongLatSource);
	BuildSettings.bSRGB = Object["bSRGB"].AsBool(BuildSettings.bSRGB);
	ReadCbField(Object["SourceEncodingOverride"], BuildSettings.SourceEncodingOverride);
	BuildSettings.bHasColorSpaceDefinition = Object["bHasColorSpaceDefinition"].AsBool(BuildSettings.bHasColorSpaceDefinition);
	ReadCbField(Object["RedChromaticityCoordinate"], BuildSettings.RedChromaticityCoordinate);
	ReadCbField(Object["GreenChromaticityCoordinate"], BuildSettings.GreenChromaticityCoordinate);
	ReadCbField(Object["BlueChromaticityCoordinate"], BuildSettings.BlueChromaticityCoordinate);
	ReadCbField(Object["WhiteChromaticityCoordinate"], BuildSettings.WhiteChromaticityCoordinate);
	ReadCbField(Object["ChromaticAdaptationMethod"], BuildSettings.ChromaticAdaptationMethod);
	BuildSettings.bUseLegacyGamma = Object["bUseLegacyGamma"].AsBool(BuildSettings.bUseLegacyGamma);
	BuildSettings.bPreserveBorder = Object["bPreserveBorder"].AsBool(BuildSettings.bPreserveBorder);
	BuildSettings.bForceNoAlphaChannel = Object["bForceNoAlphaChannel"].AsBool(BuildSettings.bForceNoAlphaChannel);
	BuildSettings.bForceAlphaChannel = Object["bForceAlphaChannel"].AsBool(BuildSettings.bForceAlphaChannel);
	BuildSettings.bComputeBokehAlpha = Object["bComputeBokehAlpha"].AsBool(BuildSettings.bComputeBokehAlpha);
	BuildSettings.bReplicateRed = Object["bReplicateRed"].AsBool(BuildSettings.bReplicateRed);
	BuildSettings.bReplicateAlpha = Object["bReplicateAlpha"].AsBool(BuildSettings.bReplicateAlpha);
	BuildSettings.bDownsampleWithAverage = Object["bDownsampleWithAverage"].AsBool(BuildSettings.bDownsampleWithAverage);
	BuildSettings.bSharpenWithoutColorShift = Object["bSharpenWithoutColorShift"].AsBool(BuildSettings.bSharpenWithoutColorShift);
	BuildSettings.bBorderColorBlack = Object["bBorderColorBlack"].AsBool(BuildSettings.bBorderColorBlack);
	BuildSettings.bFlipGreenChannel = Object["bFlipGreenChannel"].AsBool(BuildSettings.bFlipGreenChannel);
	BuildSettings.bApplyYCoCgBlockScale = Object["bApplyYCoCgBlockScale"].AsBool(BuildSettings.bApplyYCoCgBlockScale);
	BuildSettings.bApplyKernelToTopMip = Object["bApplyKernelToTopMip"].AsBool(BuildSettings.bApplyKernelToTopMip);
	BuildSettings.bRenormalizeTopMip = Object["bRenormalizeTopMip"].AsBool(BuildSettings.bRenormalizeTopMip);
	BuildSettings.bCPUAccessible = Object["bCPUAccessible"].AsBool(BuildSettings.bCPUAccessible);
	ReadCbField(Object["CompositeTextureMode"], BuildSettings.CompositeTextureMode);
	ReadCbField(Object["CompositePower"], BuildSettings.CompositePower);
	ReadCbField(Object["LODBias"], BuildSettings.LODBias);
	ReadCbField(Object["LODBiasWithCinematicMips"], BuildSettings.LODBiasWithCinematicMips);
	BuildSettings.bStreamable_Unused = Object["bStreamable"].AsBool(BuildSettings.bStreamable_Unused);
	BuildSettings.bVirtualStreamable = Object["bVirtualStreamable"].AsBool(BuildSettings.bVirtualStreamable);
	BuildSettings.bChromaKeyTexture = Object["bChromaKeyTexture"].AsBool(BuildSettings.bChromaKeyTexture);
	ReadCbField(Object["PowerOfTwoMode"], BuildSettings.PowerOfTwoMode);
	ReadCbField(Object["PaddingColor"], BuildSettings.PaddingColor);
	BuildSettings.bPadWithBorderColor = Object["bPadWithBorderColor"].AsBool(BuildSettings.bPadWithBorderColor);
	ReadCbField(Object["ResizeDuringBuildX"], BuildSettings.ResizeDuringBuildX);
	ReadCbField(Object["ResizeDuringBuildY"], BuildSettings.ResizeDuringBuildY);
	ReadCbField(Object["ChromaKeyColor"], BuildSettings.ChromaKeyColor);
	ReadCbField(Object["ChromaKeyThreshold"], BuildSettings.ChromaKeyThreshold);
	ReadCbField(Object["CompressionQuality"], BuildSettings.CompressionQuality);
	ReadCbField(Object["LossyCompressionAmount"], BuildSettings.LossyCompressionAmount);
	ReadCbField(Object["Downscale"], BuildSettings.Downscale);
	ReadCbField(Object["DownscaleOptions"], BuildSettings.DownscaleOptions);
	ReadCbField(Object["VirtualAddressingModeX"], BuildSettings.VirtualAddressingModeX);
	ReadCbField(Object["VirtualAddressingModeY"], BuildSettings.VirtualAddressingModeY);
	ReadCbField(Object["VirtualTextureTileSize"], BuildSettings.VirtualTextureTileSize);
	ReadCbField(Object["VirtualTextureBorderSize"], BuildSettings.VirtualTextureBorderSize);
	BuildSettings.OodleEncodeEffort = Object["OodleEncodeEffort"].AsUInt8(BuildSettings.OodleEncodeEffort);
	BuildSettings.OodleUniversalTiling = Object["OodleUniversalTiling"].AsUInt8(BuildSettings.OodleUniversalTiling);
	BuildSettings.bOodleUsesRDO = Object["bOodleUsesRDO"].AsBool(BuildSettings.bOodleUsesRDO);
	BuildSettings.OodleRDO = Object["OodleRDO"].AsUInt8(BuildSettings.OodleRDO);
	BuildSettings.bOodlePreserveExtremes = Object["bOodlePreserveExtremes"].AsBool(BuildSettings.bOodlePreserveExtremes);
	ReadCbField(Object["OodleTextureSdkVersion"], BuildSettings.OodleTextureSdkVersion);
	ReadCbField(Object["TextureAddressModeX"], BuildSettings.TextureAddressModeX);
	ReadCbField(Object["TextureAddressModeY"], BuildSettings.TextureAddressModeY);
	ReadCbField(Object["TextureAddressModeZ"], BuildSettings.TextureAddressModeZ);

	return BuildSettings;
}

static ERawImageFormat::Type ComputeRawImageFormat(ETextureSourceFormat SourceFormat)
{
	return FImageCoreUtils::ConvertToRawImageFormat(SourceFormat);
}

static bool TryReadTextureSourceFromCompactBinary(FCbFieldView Source, UE::DerivedData::FBuildContext& Context,
												const FTextureBuildSettings & BuildSettings, TArray<FImage>& OutMips)
{
	FSharedBuffer InputBuffer = Context.FindInput(Source.GetName());
	if (!InputBuffer)
	{
		UE_LOG(LogTextureBuildFunction, Error, TEXT("Missing input '%s'."), *WriteToString<64>(Source.GetName()));
		return false;
	}
	if ( InputBuffer.GetSize() == 0 )
	{
		UE_LOG(LogTextureBuildFunction, Error, TEXT("Input size zero '%s'."), *WriteToString<64>(Source.GetName()));
		return false;
	}

	ETextureSourceCompressionFormat CompressionFormat = (ETextureSourceCompressionFormat)Source["CompressionFormat"].AsUInt8();
	ETextureSourceFormat SourceFormat = (ETextureSourceFormat)Source["SourceFormat"].AsUInt8();

	ERawImageFormat::Type RawImageFormat = ComputeRawImageFormat(SourceFormat);

	EGammaSpace GammaSpace = (EGammaSpace)Source["GammaSpace"].AsUInt8();
	int32 NumSlices = Source["NumSlices"].AsInt32();
	int32 SizeX = Source["SizeX"].AsInt32();
	int32 SizeY = Source["SizeY"].AsInt32();
	int32 MipSizeX = SizeX;
	int32 MipSizeY = SizeY;

	const uint8* DecompressedSourceData = (const uint8*)InputBuffer.GetData();
	int64 DecompressedSourceDataSize = InputBuffer.GetSize();

	TArray64<uint8> IntermediateDecompressedData;
	if (CompressionFormat != TSCF_None)
	{
		switch (CompressionFormat)
		{
		case TSCF_JPEG:
		{
			TSharedPtr<IImageWrapper> ImageWrapper = FModuleManager::GetModuleChecked<IImageWrapperModule>(FName("ImageWrapper")).CreateImageWrapper(EImageFormat::JPEG);
			ImageWrapper->SetCompressed((const uint8*)InputBuffer.GetData(), InputBuffer.GetSize());
			ImageWrapper->GetRaw(SourceFormat == TSF_G8 ? ERGBFormat::Gray : ERGBFormat::BGRA, 8, IntermediateDecompressedData);
		}
		break;
		case TSCF_UEJPEG:
		{
			TSharedPtr<IImageWrapper> ImageWrapper = FModuleManager::GetModuleChecked<IImageWrapperModule>(FName("ImageWrapper")).CreateImageWrapper(EImageFormat::UEJPEG);
			ImageWrapper->SetCompressed((const uint8*)InputBuffer.GetData(), InputBuffer.GetSize());
			ImageWrapper->GetRaw(SourceFormat == TSF_G8 ? ERGBFormat::Gray : ERGBFormat::BGRA, 8, IntermediateDecompressedData);
		}
		break;
		case TSCF_PNG:
		{
			TSharedPtr<IImageWrapper> ImageWrapper = FModuleManager::GetModuleChecked<IImageWrapperModule>(FName("ImageWrapper")).CreateImageWrapper(EImageFormat::PNG);
			ImageWrapper->SetCompressed((const uint8*)InputBuffer.GetData(), InputBuffer.GetSize());
			ERGBFormat RawFormat = (SourceFormat == TSF_G8 || SourceFormat == TSF_G16) ? ERGBFormat::Gray : ERGBFormat::RGBA;
			ImageWrapper->GetRaw(RawFormat, (SourceFormat == TSF_G16 || SourceFormat == TSF_RGBA16) ? 16 : 8, IntermediateDecompressedData);
		}
		break;
		default:
			UE_LOG(LogTextureBuildFunction, Error, TEXT("Unexpected source compression format encountered while attempting to build a texture."));
			return false;
		}
		DecompressedSourceData = IntermediateDecompressedData.GetData();
		DecompressedSourceDataSize = IntermediateDecompressedData.Num();
		InputBuffer.Reset();
	}

	FCbArrayView MipsCbArrayView = Source["Mips"].AsArrayView();
	OutMips.Reserve(IntCastChecked<int32>(MipsCbArrayView.Num()));
	for (FCbFieldView MipsCbArrayIt : MipsCbArrayView)
	{
		FCbObjectView MipCbObjectView = MipsCbArrayIt.AsObjectView();
		int64 MipOffset = MipCbObjectView["Offset"].AsInt64();
		int64 MipSize = MipCbObjectView["Size"].AsInt64();

		FImage& SourceMip = OutMips.Emplace_GetRef(
			MipSizeX, MipSizeY,
			NumSlices,
			RawImageFormat,
			GammaSpace
		);

		check( MipOffset + MipSize <= DecompressedSourceDataSize );
		check( SourceMip.GetImageSizeBytes() == MipSize );

		if ((MipsCbArrayView.Num() == 1) && (CompressionFormat != TSCF_None))
		{
			// In the case where there is only one mip and its already in a TArray, there is no need to allocate new array contents, just use a move instead
			check( MipOffset == 0 );
			SourceMip.RawData = MoveTemp(IntermediateDecompressedData);
		}
		else
		{
			SourceMip.RawData.Reset(MipSize);
			SourceMip.RawData.AddUninitialized(MipSize);
			FMemory::Memcpy(
				SourceMip.RawData.GetData(),
				DecompressedSourceData + MipOffset,
				MipSize
			);
		}

		MipSizeX = FMath::Max(MipSizeX / 2, 1);
		MipSizeY = FMath::Max(MipSizeY / 2, 1);
		if ( BuildSettings.bVolume )
		{
			NumSlices = FMath::Max(NumSlices / 2, 1);
		}
	}

	return true;
}

FGuid FTextureBuildFunction::GetVersion() const
{
	UE::DerivedData::FBuildVersionBuilder Builder;
	Builder << TextureBuildFunctionVersion;
	ITextureFormat* TextureFormat = nullptr;
	GetVersion(Builder, TextureFormat);
	if (TextureFormat)
	{
		TArray<FName> SupportedFormats;
		TextureFormat->GetSupportedFormats(SupportedFormats);
		TArray<uint16> SupportedFormatVersions;
		for (const FName& SupportedFormat : SupportedFormats)
		{
			SupportedFormatVersions.AddUnique(TextureFormat->GetVersion(SupportedFormat));
		}
		SupportedFormatVersions.Sort();
		Builder << SupportedFormatVersions;
	}
	return Builder.Build();
}

void FTextureBuildFunction::Configure(UE::DerivedData::FBuildConfigContext& Context) const
{
	Context.SetTypeName(UTF8TEXTVIEW("Texture"));
	Context.SetCacheBucket(UE::DerivedData::FCacheBucket(ANSITEXTVIEW("Texture")));

	const FCbObject Settings = Context.FindConstant(UTF8TEXTVIEW("Settings"));
	const int64 RequiredMemoryEstimate = Settings["RequiredMemoryEstimate"].AsInt64();
	Context.SetRequiredMemory(RequiredMemoryEstimate);
}

void FTextureBuildFunction::Build(UE::DerivedData::FBuildContext& Context) const
{
	const FCbObject Settings = Context.FindConstant(UTF8TEXTVIEW("Settings"));
	if (!Settings)
	{
		UE_LOG(LogTextureBuildFunction, Error, TEXT("Settings are not available."));
		return;
	}

	const FTextureBuildSettings BuildSettings = ReadBuildSettingsFromCompactBinary(Settings["Build"].AsObjectView());
	
	const uint16 RequiredTextureFormatVersion = Settings["FormatVersion"].AsUInt16();
	const ITextureFormat* TextureFormat;
	if (ITextureFormatManagerModule* TFM = GetTextureFormatManager())
	{
		TextureFormat = TFM->FindTextureFormat(BuildSettings.TextureFormatName);
	}
	else
	{
		UE_LOG(LogTextureBuildFunction, Error, TEXT("TextureFormatManager not found!"));
		return;
	}

	const uint16 CurrentTextureFormatVersion = TextureFormat ? TextureFormat->GetVersion(BuildSettings.TextureFormatName, &BuildSettings) : 0;
	if (CurrentTextureFormatVersion != RequiredTextureFormatVersion)
	{
		UE_LOG(LogTextureBuildFunction, Error, TEXT("%s has version %hu when version %hu is required."),
			*BuildSettings.TextureFormatName.ToString(), CurrentTextureFormatVersion, RequiredTextureFormatVersion);;
		return;
	}
	
	FTextureEngineParameters EngineParameters;
	if (UE::TextureBuildUtilities::TextureEngineParameters::FromCompactBinary(EngineParameters, Context.FindConstant(UTF8TEXTVIEW("EngineParameters"))) == false)
	{
		UE_LOG(LogTextureBuildFunction, Error, TEXT("Engine parameters are not available."));
		return;
	}


	TArray<FImage> SourceMips;
	if (!TryReadTextureSourceFromCompactBinary(Settings["Source"], Context,BuildSettings, SourceMips))
	{
		return;
	}

	FSharedImageRef CPUCopy;
	if (BuildSettings.bCPUAccessible)
	{
		CPUCopy = new FSharedImage();
		SourceMips[0].CopyTo(*CPUCopy);
	
		// We just use a placeholder texture rather than the source.
		SourceMips.Empty();
		FImage& Placeholder = SourceMips.AddDefaulted_GetRef();
		UE::TextureBuildUtilities::GetPlaceholderTextureImage(&Placeholder);
	}

	TArray<FImage> AssociatedNormalSourceMips;
	if (FCbFieldView CompositeSource = Settings["CompositeSource"];
		CompositeSource && !TryReadTextureSourceFromCompactBinary(CompositeSource, Context,BuildSettings, AssociatedNormalSourceMips))
	{
		return;
	}

	// SourceMips will be cleared by BuildTexture.  Store info from it for use later.
	const int32 SourceMipsNum = SourceMips.Num();
	const int32 SourceMipsNumSlices = SourceMips[0].NumSlices;
	const int32 SourceMip0SizeX = SourceMips[0].SizeX;
	const int32 SourceMip0SizeY = SourceMips[0].SizeY;

	UE_LOG(LogTextureBuildFunction, Display, TEXT("Compressing %s -> %d source mip(s) (%dx%d) to %s..."), *Context.GetName(), SourceMipsNum, SourceMip0SizeX, SourceMip0SizeY, *BuildSettings.TextureFormatName.ToString());

	ITextureCompressorModule& TextureCompressorModule = FModuleManager::GetModuleChecked<ITextureCompressorModule>(TEXTURE_COMPRESSOR_MODULENAME);
	
	TArray<FCompressedImage2D> CompressedMips;
	uint32 NumMipsInTail;
	uint32 ExtData;
	UE::TextureBuildUtilities::FTextureBuildMetadata BuildMetadata;
	bool bBuildSucceeded = TextureCompressorModule.BuildTexture(
		SourceMips,
		AssociatedNormalSourceMips,
		BuildSettings,
		Context.GetName(),
		CompressedMips,
		NumMipsInTail,
		ExtData,
		&BuildMetadata
		);
	if (!bBuildSucceeded)
	{
		return;
	}
	check(CompressedMips.Num() > 0);


	FEncodedTextureDescription TextureDescription;

	{
		int32 CalculatedMip0SizeX = 0, CalculatedMip0SizeY = 0, CalculatedMip0NumSlices = 0;
		int32 CalculatedMipCount = TextureCompressorModule.GetMipCountForBuildSettings(SourceMip0SizeX, SourceMip0SizeY, SourceMipsNumSlices, SourceMipsNum, BuildSettings, CalculatedMip0SizeX, CalculatedMip0SizeY, CalculatedMip0NumSlices);
		BuildSettings.GetEncodedTextureDescriptionWithPixelFormat(&TextureDescription, (EPixelFormat)CompressedMips[0].PixelFormat, CalculatedMip0SizeX, CalculatedMip0SizeY, CalculatedMip0NumSlices, CalculatedMipCount);
	}

	FEncodedTextureExtendedData ExtendedData;

	// ExtendedData is only really useful for textures that have a post build step for tiling,
	// however it's possible that we ran the old build process where the tiling occurs as part
	// of the BuildTexture->CompressImage step via child texture formats. In that case, we've already
	// tiled and we need to pass the data back out. Otherwise, this gets ignored and the tiling step
	// regenerates it.
	{
		ExtendedData.NumMipsInTail = NumMipsInTail;
		ExtendedData.ExtData = ExtData;

		int32 EncodedMipCount = TextureDescription.GetNumEncodedMips(&ExtendedData);
		ExtendedData.MipSizesInBytes.AddUninitialized(EncodedMipCount);
		for (int32 MipIndex = 0; MipIndex < EncodedMipCount; MipIndex++)
		{
			ExtendedData.MipSizesInBytes[MipIndex] = CompressedMips[MipIndex].RawData.Num();
		}
	}

	// Long term, this will be supplied to the build and this would only be called to verify.
	int32 NumStreamingMips = TextureDescription.GetNumStreamingMips(&ExtendedData, EngineParameters);
	
	{
		if (CPUCopy.IsValid())
		{
			FCbObject ImageInfoMetadata;
			CPUCopy->ImageInfoToCompactBinary(ImageInfoMetadata);
			Context.AddValue(UE::DerivedData::FValueId::FromName(ANSITEXTVIEW("CPUCopyImageInfo")), ImageInfoMetadata);

			FSharedBuffer CPUCopyData = MakeSharedBufferFromArray(MoveTemp(CPUCopy->RawData));
			Context.AddValue(UE::DerivedData::FValueId::FromName(ANSITEXTVIEW("CPUCopyRawData")), CPUCopyData);
		}

		// This will get added to the build metadata in a later cl.
		// Context.AddValue(UE::DerivedData::FValueId::FromName(ANSITEXTVIEW("TextureBuildMetadata")), BuildMetadata.ToCompactBinaryWithDefaults());
		Context.AddValue(UE::DerivedData::FValueId::FromName(ANSITEXTVIEW("EncodedTextureDescription")), UE::TextureBuildUtilities::EncodedTextureDescription::ToCompactBinary(TextureDescription));
		Context.AddValue(UE::DerivedData::FValueId::FromName(ANSITEXTVIEW("EncodedTextureExtendedData")), UE::TextureBuildUtilities::EncodedTextureExtendedData::ToCompactBinary(ExtendedData));

		// Streaming mips
		for (int32 MipIndex = 0; MipIndex < NumStreamingMips; ++MipIndex)
		{
			TAnsiStringBuilder<16> MipName;
			MipName << ANSITEXTVIEW("Mip") << MipIndex;

			FSharedBuffer MipData = MakeSharedBufferFromArray(MoveTemp(CompressedMips[MipIndex].RawData));
			Context.AddValue(UE::DerivedData::FValueId::FromName(MipName), MipData);
		}

		// Mip tail
		TArray<FSharedBuffer> MipTailComponents;
		for (int32 MipIndex = NumStreamingMips; MipIndex < TextureDescription.NumMips; ++MipIndex)
		{
			FSharedBuffer MipData = MakeSharedBufferFromArray(MoveTemp(CompressedMips[MipIndex].RawData));
			MipTailComponents.Add(MipData);
		}
		FCompositeBuffer MipTail(MipTailComponents);
		if (MipTail.GetSize() > 0)
		{
			Context.AddValue(UE::DerivedData::FValueId::FromName(ANSITEXTVIEW("MipTail")), MipTail);
		}
	}
}

void GenericTextureTilingBuildFunction(UE::DerivedData::FBuildContext& Context, const ITextureTiler* Tiler, const UE::DerivedData::FUtf8SharedString& BuildFunctionName)
{
	// The texture description is either passed as a constant or as an output from the other build ("build input").
	FEncodedTextureDescription TextureDescription;
	{
		FCbObject TextureDescriptionCb = Context.FindConstant(UTF8TEXTVIEW("EncodedTextureDescriptionConstant"));
		if (!TextureDescriptionCb)
		{
			FSharedBuffer RawTextureDescription = Context.FindInput(UTF8TEXTVIEW("EncodedTextureDescriptionInput"));
			if (!RawTextureDescription)
			{
				return;
			}
			TextureDescriptionCb = FCbObject(RawTextureDescription);
		}
		UE::TextureBuildUtilities::EncodedTextureDescription::FromCompactBinary(TextureDescription, TextureDescriptionCb);
	}

	// The extended data is either passed as a constant, but is not output from the linear build - it's
	// our job to make it.
	FEncodedTextureExtendedData TextureExtendedData;
	{
		FCbObject TextureExtendedDataCb = Context.FindConstant(UTF8TEXTVIEW("EncodedTextureExtendedDataConstant"));
		if (TextureExtendedDataCb)
		{
			UE::TextureBuildUtilities::EncodedTextureExtendedData::FromCompactBinary(TextureExtendedData, TextureExtendedDataCb);
		}
		else
		{
			// If we're in this path we need to have the LODBias delivered to us.
			FCbObject LODBiasCb = Context.FindConstant(UTF8TEXTVIEW("LODBias"));
			TextureExtendedData = Tiler->GetExtendedDataForTexture(TextureDescription, LODBiasCb["LODBias"].AsInt8());
		}
	}

	FTextureEngineParameters EngineParameters;
	{
		FCbObject EngineParametersCb = Context.FindConstant(UTF8TEXTVIEW("EngineParameters"));
		UE::TextureBuildUtilities::TextureEngineParameters::FromCompactBinary(EngineParameters, EngineParametersCb);
	}

	// This will get added to the build metadata in a later cl.
	//UE::TextureBuildUtilities::FTextureBuildMetadata BuildMetadata(FCbObject(Context.FindInput(ANSITEXTVIEW("TextureBuildMetadata"))));

	UE_LOG(LogTextureBuildFunction, Display, TEXT("Tiling %s with %s -> %d source mip(s) with a tail of %d..."), *Context.GetName(), StringCast<TCHAR>(*BuildFunctionName).Get(), TextureDescription.NumMips, TextureExtendedData.NumMipsInTail);

	//
	// Careful - the linear build might have a different streaming mip count than we output due to mip tail
	// packing.
	//
	int32 InputTextureNumStreamingMips = TextureDescription.GetNumStreamingMips(nullptr, EngineParameters);
	int32 OutputTextureNumStreamingMips = TextureDescription.GetNumStreamingMips(&TextureExtendedData, EngineParameters);

	FSharedBuffer InputTextureMipTailData;
	if (TextureDescription.NumMips > InputTextureNumStreamingMips)
	{
		InputTextureMipTailData = Context.FindInput(UTF8TEXTVIEW("MipTail"));
	}

	// We might be packing several mips in to a single tiled mip at the end, so we need to have all the buffers available
	// to potentially pass to ProcessMipLevel. Can do this on demand so that the highest mip level isn't in memory for the entire
	// mip chain... however all the time is also spent on it and it's only +33% size for the entire chain, so not really worth.
	TArray<FSharedBuffer> InputTextureMipBuffers;
	TArray<FMemoryView> InputTextureMipViews;

	uint64 CurrentMipTailOffset = 0;
	for (int32 MipIndex = 0; MipIndex < TextureDescription.NumMips; MipIndex++)
	{
		FMemoryView SourceMipView;
		if (MipIndex >= InputTextureNumStreamingMips)
		{
			// Mip tail.
			uint64 SourceMipSize = TextureDescription.GetMipSizeInBytes(MipIndex);
			SourceMipView = InputTextureMipTailData.GetView().Mid(CurrentMipTailOffset, SourceMipSize);
			CurrentMipTailOffset += SourceMipSize;
		}
		else
		{
			TUtf8StringBuilder<10> StreamingMipName;
			StreamingMipName << "Mip" << MipIndex;

			FSharedBuffer SourceData = Context.FindInput(StreamingMipName);
			check(SourceData.GetSize() == TextureDescription.GetMipSizeInBytes(MipIndex));
			SourceMipView = SourceData.GetView();
			InputTextureMipBuffers.Add(SourceData);
		}
		InputTextureMipViews.Add(SourceMipView);
	}

	// If the platform packs mip tails, we need to pass all the relevant mip buffers at once.
	int32 FirstMipTailIndex = TextureDescription.NumMips - 1;
	int32 MipTailCount = 1;
	if (TextureExtendedData.NumMipsInTail > 1)
	{
		MipTailCount = TextureExtendedData.NumMipsInTail;
		FirstMipTailIndex = TextureDescription.NumMips - MipTailCount;
	}

	// Process the mips
	TArray<FSharedBuffer> MipTailBuffers;
	for (int32 MipIndex = 0; MipIndex < FirstMipTailIndex + 1; MipIndex++)
	{
		TUtf8StringBuilder<10> StreamingMipName;
		StreamingMipName << "Mip" << MipIndex;

		TArrayView<FMemoryView> MipsForLevel = MakeArrayView(InputTextureMipViews.GetData() + MipIndex, 1);
		if (MipIndex == FirstMipTailIndex)
		{
			MipsForLevel = MakeArrayView(InputTextureMipViews.GetData() + MipIndex, MipTailCount);
		}
		FSharedBuffer MipData = Tiler->ProcessMipLevel(TextureDescription, TextureExtendedData, MipsForLevel, MipIndex);

		// Make sure we got the size we advertised prior to the build. If this ever fires then we
		// have a critical mismatch!
		check(TextureExtendedData.MipSizesInBytes[MipIndex] == MipData.GetSize());

		// Save the data to the output.
		if (MipIndex < OutputTextureNumStreamingMips)
		{
			Context.AddValue(UE::DerivedData::FValueId::FromName(StreamingMipName), MipData);
		}
		else
		{
			MipTailBuffers.Add(MipData);
		}
	} // end for each mip

	// The mip tail is a bunch of mips all together in one "Value", so assemble them here.
	FCompositeBuffer MipTail(MipTailBuffers);
	if (MipTail.GetSize() > 0)
	{
		Context.AddValue(UE::DerivedData::FValueId::FromName(UTF8TEXTVIEW("MipTail")), MipTail);
	}

	Context.AddValue(UE::DerivedData::FValueId::FromName(UTF8TEXTVIEW("EncodedTextureDescription")), UE::TextureBuildUtilities::EncodedTextureDescription::ToCompactBinary(TextureDescription));
	Context.AddValue(UE::DerivedData::FValueId::FromName(UTF8TEXTVIEW("EncodedTextureExtendedData")), UE::TextureBuildUtilities::EncodedTextureExtendedData::ToCompactBinary(TextureExtendedData));
	// This will get added to the build metadata in a later cl.
	//Context.AddValue(UE::DerivedData::FValueId::FromName(UTF8TEXTVIEW("TextureBuildMetadata")), BuildMetadata.ToCompactBinaryWithDefaults());
}
