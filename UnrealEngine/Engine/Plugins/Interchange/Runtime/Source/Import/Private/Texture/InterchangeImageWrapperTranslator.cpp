// Copyright Epic Games, Inc. All Rights Reserved. 

#include "Texture/InterchangeImageWrapperTranslator.h"

#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageCoreUtils.h"
#include "InterchangeImportLog.h"
#include "InterchangeTextureNode.h"
#include "Memory/SharedBuffer.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Texture/TextureTranslatorUtilities.h"
#include "TextureImportUtils.h"
#include "TgaImageSupport.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeImageWrapperTranslator)

static bool GInterchangeEnablePNGImport = true;
static FAutoConsoleVariableRef CCvarInterchangeEnablePNGImport(
	TEXT("Interchange.FeatureFlags.Import.PNG"),
	GInterchangeEnablePNGImport,
	TEXT("Whether PNG support is enabled."),
	ECVF_Default);

static bool GInterchangeEnableBMPImport = true;
static FAutoConsoleVariableRef CCvarInterchangeEnableBMPImport(
	TEXT("Interchange.FeatureFlags.Import.BMP"),
	GInterchangeEnableBMPImport,
	TEXT("Whether BMP support is enabled."),
	ECVF_Default);

static bool GInterchangeEnableEXRImport = true;
static FAutoConsoleVariableRef CCvarInterchangeEnableEXRImport(
	TEXT("Interchange.FeatureFlags.Import.EXR"),
	GInterchangeEnableEXRImport,
	TEXT("Whether OpenEXR support is enabled."),
	ECVF_Default);

static bool GInterchangeEnableHDRImport = true;
static FAutoConsoleVariableRef CCvarInterchangeEnableHDRImport(
	TEXT("Interchange.FeatureFlags.Import.HDR"),
	GInterchangeEnableHDRImport,
	TEXT("Whether HDR support is enabled."),
	ECVF_Default);

#if WITH_LIBTIFF
static bool GInterchangeEnableTIFFImport = true;
static FAutoConsoleVariableRef CCvarInterchangeEnableTIFFImport(
	TEXT("Interchange.FeatureFlags.Import.TIFF"),
	GInterchangeEnableTIFFImport,
	TEXT("Whether TIFF support is enabled."),
	ECVF_Default);
#endif

static bool GInterchangeEnableTGAImport = true;
static FAutoConsoleVariableRef CCvarInterchangeEnableTGAImport(
	TEXT("Interchange.FeatureFlags.Import.TGA"),
	GInterchangeEnableTGAImport,
	TEXT("Whether TGA support is enabled."),
	ECVF_Default);

UInterchangeImageWrapperTranslator::UInterchangeImageWrapperTranslator()
{
	if (IsTemplate() && GConfig)
	{
		ensure(IsInGameThread()); // Reading config values isn't threadsafe
		bFillPNGZeroAlpha = true;
		GConfig->GetBool(TEXT("TextureImporter"), TEXT("FillPNGZeroAlpha"), bFillPNGZeroAlpha.GetValue(), GEditorIni);
	}
}

TArray<FString> UInterchangeImageWrapperTranslator::GetSupportedFormats() const
{
	TArray<FString> Formats;
	Formats.Reserve(7);

	if (GInterchangeEnablePNGImport || GIsAutomationTesting)
	{
		Formats.Emplace(TEXT("png;Portable Network Graphic"));
	}

	if (GInterchangeEnableBMPImport || GIsAutomationTesting)
	{
		Formats.Emplace(TEXT("bmp;Bitmap image"));
	}

	if (GInterchangeEnableEXRImport || GIsAutomationTesting)
	{
		Formats.Emplace(TEXT("exr;OpenEXR image"));
	}

	if (GInterchangeEnableHDRImport || GIsAutomationTesting)
	{
		Formats.Emplace(TEXT("hdr;High Dynamic Range image"));
	}

#if WITH_LIBTIFF
	if (GInterchangeEnableTIFFImport || GIsAutomationTesting)
	{
		Formats.Emplace(TEXT("tif;Tag Image File Format"));
		Formats.Emplace(TEXT("tiff;Tag Image File Format"));
	}
#endif

	if (GInterchangeEnableTGAImport || GIsAutomationTesting)
	{
		Formats.Emplace(TEXT("tga;Targa image"));
	}

	return Formats;
}

bool UInterchangeImageWrapperTranslator::Translate(UInterchangeBaseNodeContainer& BaseNodeContainer) const
{
	return UE::Interchange::FTextureTranslatorUtilities::Generic2DTextureTranslate(GetSourceData(), BaseNodeContainer);
}

TOptional<UE::Interchange::FImportImage> UInterchangeImageWrapperTranslator::GetTexturePayloadData(const UInterchangeSourceData* PayloadSourceData, const FString& PayLoadKey) const
{
	check(PayloadSourceData == GetSourceData());

	if (!GetSourceData())
	{
		UE_LOG(LogInterchangeImport, Error, TEXT("Failed to import PNG, bad source data."));
		return TOptional<UE::Interchange::FImportImage>();
	}

	TArray64<uint8> SourceDataBuffer;
	FString Filename = GetSourceData()->GetFilename();

	//Make sure the key fit the filename, The key should always be valid
	if (!Filename.Equals(PayLoadKey))
	{
		UE_LOG(LogInterchangeImport, Error, TEXT("Failed to import PNG, wrong payload key. [%s]"), *Filename);
		return TOptional<UE::Interchange::FImportImage>();
	}

	if (!FPaths::FileExists(Filename))
	{
		UE_LOG(LogInterchangeImport, Error, TEXT("Failed to import PNG, cannot open file. [%s]"), *Filename);
		return TOptional<UE::Interchange::FImportImage>();
	}

	if (!FFileHelper::LoadFileToArray(SourceDataBuffer, *Filename))
	{
		UE_LOG(LogInterchangeImport, Error, TEXT("Failed to import PNG, cannot load file content into an array. [%s]"), *Filename);
		return TOptional<UE::Interchange::FImportImage>();
	}

	return GetTexturePayloadDataFromBuffer(SourceDataBuffer);
}

TOptional<UE::Interchange::FImportImage> UInterchangeImageWrapperTranslator::GetTexturePayloadDataFromBuffer(const TArray64<uint8>& SourceDataBuffer) const
{
	const uint8* Buffer = SourceDataBuffer.GetData();
	const uint8* BufferEnd = Buffer + SourceDataBuffer.Num();

	const int32 Length = BufferEnd - Buffer;

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

	EImageFormat ImageFormat = ImageWrapperModule.DetectImageFormat(Buffer, Length);


	UE::Interchange::FImportImage PayloadData;

	if (ImageFormat != EImageFormat::Invalid)
	{
		// Generic ImageWrapper loader :
		// for PNG,EXR,BMP,TGA :
		FImage LoadedImage;
		if (ImageWrapperModule.DecompressImage(Buffer, Length, LoadedImage))
		{
			// Todo interchange: should these payload modification be part of the pipeline, factory or stay there?
			if (UE::TextureUtilitiesCommon::AutoDetectAndChangeGrayScale(LoadedImage))
			{
				UE_LOG(LogInterchangeImport, Display, TEXT("Auto-detected grayscale, image changed to G8"));
			}

			ETextureSourceFormat TextureFormat = FImageCoreUtils::ConvertToTextureSourceFormat(LoadedImage.Format);
			bool bSRGB = LoadedImage.GammaSpace != EGammaSpace::Linear;

			PayloadData.Init2DWithParams(
				LoadedImage.SizeX,
				LoadedImage.SizeY,
				TextureFormat,
				bSRGB,
				false
			);

			PayloadData.RawData = MakeUniqueBufferFromArray(MoveTemp(LoadedImage.RawData));

			if (ERawImageFormat::IsHDR(LoadedImage.Format))
			{
				PayloadData.CompressionSettings = TC_HDR;
				check(bSRGB == false);
			}

			// do per-format processing to match legacy behavior :

			if (ImageFormat == EImageFormat::PNG)
			{
				if (GetDefault<UInterchangeImageWrapperTranslator>()->bFillPNGZeroAlpha.GetValue())
				{
					// Replace the pixels with 0.0 alpha with a color value from the nearest neighboring color which has a non-zero alpha
					UE::TextureUtilitiesCommon::FillZeroAlphaPNGData(PayloadData.SizeX, PayloadData.SizeY, PayloadData.Format, reinterpret_cast<uint8*>(PayloadData.RawData.GetData()));
				}
			}
			else if (ImageFormat == EImageFormat::TGA)
			{
				const FTGAFileHeader* TGA = (FTGAFileHeader*)Buffer;

				if (TGA->ColorMapType == 1 && TGA->ImageTypeCode == 1 && TGA->BitsPerPixel == 8)
				{
					// Notes: The Scaleform GFx exporter (dll) strips all font glyphs into a single 8-bit texture.
					// The targa format uses this for a palette index; GFx uses a palette of (i,i,i,i) so the index
					// is also the alpha value.
					//
					// We store the image as PF_G8, where it will be used as alpha in the Glyph shader.

					// ?? check or convert? or neither?
					//check( TextureFormat == TSF_G8 );

					PayloadData.CompressionSettings = TC_Grayscale;
				}
				else if (TGA->ColorMapType == 0 && TGA->ImageTypeCode == 3 && TGA->BitsPerPixel == 8)
				{
					// standard grayscale images

					// ?? check or convert? or neither?
					//check( TextureFormat == TSF_G8 );

					PayloadData.CompressionSettings = TC_Grayscale;
				}

				if (PayloadData.CompressionSettings == TC_Grayscale && TGA->ImageTypeCode == 3)
				{
					// default grayscales to linear as they wont get compression otherwise and are commonly used as masks
					PayloadData.bSRGB = false;
				}
			}
		}
	}

	return PayloadData;
}

