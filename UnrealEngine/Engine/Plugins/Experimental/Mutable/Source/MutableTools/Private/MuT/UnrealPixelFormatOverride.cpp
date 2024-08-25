// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/UnrealPixelFormatOverride.h"

#include "Interfaces/ITextureFormatManagerModule.h"
#include "Interfaces/ITextureFormatModule.h"
#include "Modules/ModuleManager.h"
#include "Engine/TextureDefines.h"
#include "TextureCompressorModule.h"
#include "TextureBuildUtilities.h"
#include "ImageCore.h"


static ITextureFormatManagerModule* STextureFormatManager = nullptr;


void PrepareUnrealCompression()
{
	check(IsInGameThread());

	if (!STextureFormatManager)
	{
		STextureFormatManager = &FModuleManager::LoadModuleChecked<ITextureFormatManagerModule>("TextureFormat");
		check(STextureFormatManager);
	}
}


void FillBuildSettingsFromMutableFormat(FTextureBuildSettings& Settings, bool& bOutHasAlpha, mu::EImageFormat Format)
{
	Settings.MipGenSettings = TMGS_NoMipmaps;

	switch (Format)
	{
	case mu::EImageFormat::IF_ASTC_4x4_RGBA_LDR:
		Settings.TextureFormatName = TEXT("ASTC_RGBA_HQ");
		Settings.CompressionQuality = 4; // See GetQualityFormat in TextureFormatASTC.cpp
		bOutHasAlpha = true;
		break;
	case mu::EImageFormat::IF_ASTC_6x6_RGBA_LDR:
		Settings.TextureFormatName = TEXT("ASTC_RGBA");
		Settings.CompressionQuality = 3; // See GetQualityFormat in TextureFormatASTC.cpp
		bOutHasAlpha = true;
		break;
	case mu::EImageFormat::IF_ASTC_8x8_RGBA_LDR:
		Settings.TextureFormatName = TEXT("ASTC_RGBA");
		Settings.CompressionQuality = 2; // See GetQualityFormat in TextureFormatASTC.cpp
		bOutHasAlpha = true;
		break;
	case mu::EImageFormat::IF_ASTC_10x10_RGBA_LDR:
		Settings.TextureFormatName = TEXT("ASTC_RGBA");
		Settings.CompressionQuality = 1; // See GetQualityFormat in TextureFormatASTC.cpp
		bOutHasAlpha = true;
		break;
	case mu::EImageFormat::IF_ASTC_12x12_RGBA_LDR:
		Settings.TextureFormatName = TEXT("ASTC_RGBA");
		Settings.CompressionQuality = 0; // See GetQualityFormat in TextureFormatASTC.cpp
		bOutHasAlpha = true;
		break;

	case mu::EImageFormat::IF_ASTC_4x4_RGB_LDR:
		Settings.TextureFormatName = TEXT("ASTC_RGBA_HQ");
		Settings.CompressionQuality = 4; // See GetQualityFormat in TextureFormatASTC.cpp
		bOutHasAlpha = false;
		break;
	case mu::EImageFormat::IF_ASTC_6x6_RGB_LDR:
		Settings.TextureFormatName = TEXT("ASTC_RGBA");
		Settings.CompressionQuality = 3; // See GetQualityFormat in TextureFormatASTC.cpp
		bOutHasAlpha = false;
		break;
	case mu::EImageFormat::IF_ASTC_8x8_RGB_LDR:
		Settings.TextureFormatName = TEXT("ASTC_RGBA");
		Settings.CompressionQuality = 2; // See GetQualityFormat in TextureFormatASTC.cpp
		bOutHasAlpha = false;
		break;
	case mu::EImageFormat::IF_ASTC_10x10_RGB_LDR:
		Settings.TextureFormatName = TEXT("ASTC_RGBA");
		Settings.CompressionQuality = 1; // See GetQualityFormat in TextureFormatASTC.cpp
		bOutHasAlpha = false;
		break;
	case mu::EImageFormat::IF_ASTC_12x12_RGB_LDR:
		Settings.TextureFormatName = TEXT("ASTC_RGBA");
		Settings.CompressionQuality = 0; // See GetQualityFormat in TextureFormatASTC.cpp
		bOutHasAlpha = false;
		break;

	case mu::EImageFormat::IF_ASTC_4x4_RG_LDR:
		Settings.TextureFormatName = TEXT("ASTC_NormalRG_Precise");
		Settings.CompressionQuality = 4; // See GetQualityFormat in TextureFormatASTC.cpp
		bOutHasAlpha = false;
		break;
	case mu::EImageFormat::IF_ASTC_6x6_RG_LDR:
		Settings.TextureFormatName = TEXT("ASTC_NormalRG");
		Settings.CompressionQuality = 3; // See GetQualityFormat in TextureFormatASTC.cpp
		bOutHasAlpha = false;
		break;
	case mu::EImageFormat::IF_ASTC_8x8_RG_LDR:
		Settings.TextureFormatName = TEXT("ASTC_NormalLA");
		Settings.CompressionQuality = 2; // See GetQualityFormat in TextureFormatASTC.cpp
		bOutHasAlpha = false;
		break;
	case mu::EImageFormat::IF_ASTC_10x10_RG_LDR:
		Settings.TextureFormatName = TEXT("ASTC_NormalRG");
		Settings.CompressionQuality = 1; // See GetQualityFormat in TextureFormatASTC.cpp
		bOutHasAlpha = false;
		break;
	case mu::EImageFormat::IF_ASTC_12x12_RG_LDR:
		Settings.TextureFormatName = TEXT("ASTC_NormalRG");
		Settings.CompressionQuality = 0; // See GetQualityFormat in TextureFormatASTC.cpp
		bOutHasAlpha = false;
		break;

	case mu::EImageFormat::IF_BC1:
		Settings.TextureFormatName = TEXT("DXT1");
		bOutHasAlpha = false;
		break;
	case mu::EImageFormat::IF_BC2:
		Settings.TextureFormatName = TEXT("DXT3");
		bOutHasAlpha = true;
		break;
	case mu::EImageFormat::IF_BC3:
		Settings.TextureFormatName = TEXT("DXT5");
		bOutHasAlpha = true;
		break;
	case mu::EImageFormat::IF_BC4:
		Settings.TextureFormatName = TEXT("BC4");
		bOutHasAlpha = false;
		break;
	case mu::EImageFormat::IF_BC5:
		Settings.TextureFormatName = TEXT("BC5");
		bOutHasAlpha = true;
		break;

	default:
		Settings.TextureFormatName = NAME_None;
		bOutHasAlpha = false;
		break;
	}
}


void MutableToImageCore(const mu::Image* InMutable, FImage& CoreImage, int32 LOD)
{
	MUTABLE_CPUPROFILER_SCOPE(MutableToImageCore);

	mu::Ptr<const mu::Image> Mutable = InMutable;

	ERawImageFormat::Type CoreImageFormat;
	switch (Mutable->GetFormat())
	{
	case mu::EImageFormat::IF_BGRA_UBYTE: CoreImageFormat = ERawImageFormat::BGRA8; break;
		//case mu::EImageFormat::IF_L_UBYTE: CoreImageFormat = ERawImageFormat::G8; break;

	default:
	{
		// Unsupported format: force conversion
		mu::FImageOperator ImOp = mu::FImageOperator::GetDefault(mu::FImageOperator::FImagePixelFormatFunc());
		Mutable = ImOp.ImagePixelFormat(4, InMutable, mu::EImageFormat::IF_BGRA_UBYTE);
		CoreImageFormat = ERawImageFormat::BGRA8;
		break;
	}

	}

	FIntVector2 MipSize = Mutable->CalculateMipSize(LOD);
	CoreImage.Init(MipSize.X, MipSize.Y, CoreImageFormat, EGammaSpace::Linear);
	FMemory::Memcpy(CoreImage.RawData.GetData(), Mutable->GetMipData(LOD), CoreImage.GetImageSizeBytes());
}


void ImageCoreToMutable(const FCompressedImage2D& Compressed, mu::Image* Mutable, int32 LOD)
{
	TArrayView<uint8> MutableView = Mutable->DataStorage.GetLOD(LOD);

	check(Compressed.RawData.Num() == MutableView.Num());
	FMemory::Memcpy(MutableView.GetData(), Compressed.RawData.GetData(), Compressed.RawData.Num());
}


mu::EImageFormat UnrealToMutablePixelFormat(EPixelFormat PlatformFormat, bool bHasAlpha)
{
	switch (PlatformFormat)
	{
	case PF_ASTC_4x4: return bHasAlpha ? mu::EImageFormat::IF_ASTC_4x4_RGBA_LDR : mu::EImageFormat::IF_ASTC_4x4_RGB_LDR;
	case PF_ASTC_6x6: return bHasAlpha ? mu::EImageFormat::IF_ASTC_6x6_RGBA_LDR : mu::EImageFormat::IF_ASTC_6x6_RGB_LDR;
	case PF_ASTC_8x8: return bHasAlpha ? mu::EImageFormat::IF_ASTC_8x8_RGBA_LDR : mu::EImageFormat::IF_ASTC_8x8_RGB_LDR;
	case PF_ASTC_10x10: return bHasAlpha ? mu::EImageFormat::IF_ASTC_10x10_RGBA_LDR : mu::EImageFormat::IF_ASTC_10x10_RGB_LDR;
	case PF_ASTC_12x12: return bHasAlpha ? mu::EImageFormat::IF_ASTC_12x12_RGBA_LDR : mu::EImageFormat::IF_ASTC_12x12_RGB_LDR;
	case PF_ASTC_4x4_NORM_RG: return mu::EImageFormat::IF_ASTC_4x4_RG_LDR;
	case PF_ASTC_6x6_NORM_RG: return mu::EImageFormat::IF_ASTC_6x6_RG_LDR;
	case PF_ASTC_8x8_NORM_RG: return mu::EImageFormat::IF_ASTC_8x8_RG_LDR;
	case PF_ASTC_10x10_NORM_RG: return mu::EImageFormat::IF_ASTC_10x10_RG_LDR;
	case PF_ASTC_12x12_NORM_RG: return mu::EImageFormat::IF_ASTC_12x12_RG_LDR;
	case PF_DXT1: return mu::EImageFormat::IF_BC1;
	case PF_DXT3: return mu::EImageFormat::IF_BC2;
	case PF_DXT5: return mu::EImageFormat::IF_BC3;
	case PF_BC4: return mu::EImageFormat::IF_BC4;
	case PF_BC5: return mu::EImageFormat::IF_BC5;
	case PF_G8: return mu::EImageFormat::IF_L_UBYTE;
	case PF_L8: return mu::EImageFormat::IF_L_UBYTE;
	case PF_A8: return mu::EImageFormat::IF_L_UBYTE;
	case PF_R8G8B8A8: return mu::EImageFormat::IF_RGBA_UBYTE;
	case PF_A8R8G8B8: return mu::EImageFormat::IF_RGBA_UBYTE;
	case PF_B8G8R8A8: return mu::EImageFormat::IF_BGRA_UBYTE;
	default:
		return mu::EImageFormat::IF_NONE;
	}
}


mu::EImageFormat QualityAndPerformanceFix(mu::EImageFormat Format)
{
	switch (Format)
	{
	case mu::EImageFormat::IF_ASTC_8x8_RGB_LDR:		return mu::EImageFormat::IF_ASTC_4x4_RGB_LDR; break;
	case mu::EImageFormat::IF_ASTC_8x8_RGBA_LDR:	return mu::EImageFormat::IF_ASTC_4x4_RGBA_LDR; break;
	case mu::EImageFormat::IF_ASTC_8x8_RG_LDR:		return mu::EImageFormat::IF_ASTC_4x4_RG_LDR; break;
	case mu::EImageFormat::IF_ASTC_12x12_RGB_LDR:	return mu::EImageFormat::IF_ASTC_4x4_RGB_LDR; break;
	case mu::EImageFormat::IF_ASTC_12x12_RGBA_LDR:	return mu::EImageFormat::IF_ASTC_4x4_RGBA_LDR; break;
	case mu::EImageFormat::IF_ASTC_12x12_RG_LDR:	return mu::EImageFormat::IF_ASTC_4x4_RG_LDR; break;
	case mu::EImageFormat::IF_ASTC_6x6_RGB_LDR:		return mu::EImageFormat::IF_ASTC_4x4_RGB_LDR; break;
	case mu::EImageFormat::IF_ASTC_6x6_RGBA_LDR:	return mu::EImageFormat::IF_ASTC_4x4_RGBA_LDR; break;
	case mu::EImageFormat::IF_ASTC_6x6_RG_LDR:		return mu::EImageFormat::IF_ASTC_4x4_RG_LDR; break;
	case mu::EImageFormat::IF_ASTC_10x10_RGB_LDR:	return mu::EImageFormat::IF_ASTC_4x4_RGB_LDR; break;
	case mu::EImageFormat::IF_ASTC_10x10_RGBA_LDR:	return mu::EImageFormat::IF_ASTC_4x4_RGBA_LDR; break;
	case mu::EImageFormat::IF_ASTC_10x10_RG_LDR:	return mu::EImageFormat::IF_ASTC_4x4_RG_LDR; break;

	// This is more of a performance fix.
	case mu::EImageFormat::IF_BGRA_UBYTE:			return mu::EImageFormat::IF_RGBA_UBYTE;

	default:
		break;
	}

	return Format;
}


void UnrealPixelFormatFunc(bool& bOutSuccess, int32 Quality, mu::Image* Target, const mu::Image* Source, int32 OnlyLOD)
{
	// If this fails, PrepareUnrealCompression wasn't called before.
	check(STextureFormatManager);

	bOutSuccess = true;

	FTextureBuildSettings Settings;
	bool bHasAlpha = false;
	FillBuildSettingsFromMutableFormat(Settings, bHasAlpha, Target->GetFormat());

	if (Settings.TextureFormatName == NAME_None)
	{
		// Unsupported format in the override: use standard mutable compression.
		bOutSuccess = false;
		return;
	}

	const ITextureFormat* TextureFormat = STextureFormatManager->FindTextureFormat(Settings.TextureFormatName);
	check(TextureFormat);

	int32 FirstLOD = 0;
	int32 LODCount = Source->GetLODCount();
	if (OnlyLOD >= 0)
	{
		FirstLOD = OnlyLOD;
		LODCount = 1;
	}

	for (int32 LOD = FirstLOD; LOD < LODCount; ++LOD)
	{
		FImage SourceUnreal;
		MutableToImageCore(Source, SourceUnreal, LOD);

		FCompressedImage2D CompressedUnreal;
		TextureFormat->CompressImage(SourceUnreal, Settings,
			FIntVector3(SourceUnreal.SizeX, SourceUnreal.SizeY, 1),
			0, 0, 1,
			FString(),
			bHasAlpha, CompressedUnreal);

		ImageCoreToMutable(CompressedUnreal, Target, LOD);
	}
}

